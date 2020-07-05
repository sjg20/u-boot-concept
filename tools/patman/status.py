# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2020 Google LLC
#
# This talks to the patchwork service to figure out what patches have been
# reviewed and commented on.

from bs4 import BeautifulSoup
import collections
import concurrent.futures
import io
import pygit2
from itertools import repeat
import re
import requests
import time
from urllib.parse import urljoin

from patman import commit
from patman import gitutil
from patman import patchstream
from patman import terminal
from patman.terminal import Color

# Patches which are part of a multi-patch series are shown with a prefix like
# [prefix, version, sequence], for example '[RFC, v2, 3/5]'. All but the last
# part is optional. This decodes the string into groups. For single patches
# the [] part is not present:
# Groups: (ignore, ignore, ignore, prefix, version, sequence, subject)
re_patch = re.compile('(\[(((.*),)?(.*),)?(.*)\]\s)?(.*)$')

# This decodes the sequence string into a patch number and patch count
re_seq = re.compile('(\d+)/(\d+)')

def to_int(vals):
    """Convert a lsit of strings into integers, using 0 if not an integer

    Args:
        vals: List of strings

    Returns:
        List of integers, one for each input string
    """
    out = [int(val) if val.isdigit() else 0 for val in vals]
    return out


class Patch(dict):
    """Models a patch in patchwork

    This class records information obtained from patchwork

    Some of this information comes from the 'Patch' column:

        [RFC,v2,1/3] dm: Driver and uclass changes for tiny-dm

    This shows the prefix, version, seq, count and subject.

    The other properties come from other columns in the display.

    Properties:
        seq: Sequence number within series (1=first) parsed from sequence string
        count: Number of patches in series, parsed from sequence string
        prefix: Prefix string or None
        subject: Patch subject with [..] part removed (same as commit subject)
        series: Series name
        acked: Number of Acked-by rtags
        fixes: Number of Fixes rtags
        reviewed: Number of Reviewed-by rtags
        tested: Number of Tested-by rtags
        success: Success value
        warning: Warning value
        fail: Failure value
        date: Date as a string
        Submitter: Submitter name
        Delegate: Deleted ID
        state: Current state (e.g. 'New')
        url: URL pointing to the patch on patchwork
    """
    def __init__(self):
        self.seq = None
        self.count = None
        self.prefix = None
        self.subject = None
        self.series = None
        self.acked = None
        self.fixes = None
        self.reviewed = None
        self.tested = None
        self.success = None
        self.warning = None
        self.fail = None
        self.date = None
        self.submitter = None
        self.delegate = None
        self.state = None
        self.url = None

    # These make us more like a dictionary
    def __setattr__(self, name, value):
        self[name] = value

    def __getattr__(self, name):
        return self[name]

    def Add(self, name, value):
        """Add information obtained from the web page

        This sets the object properties based on @name.

        Args:
            name: Column name in lower-case, e.g. 'Series' or 'State'
            value: Text obtained for this patch from that column, e.g. 'New'
        """
        if name in self:
            self[name] = value
        elif name == 'patch':
            mat = re_patch.search(value.strip())
            if not mat:
                raise ValueError("Cannot parse subject '%s'" % value)
            self.prefix = mat.group(1)
            self.prefix, self.version, seq_info, self.subject = mat.groups()[3:]
            mat_seq = re_seq.match(seq_info)
            if not mat_seq:
                raise ValueError("Cannot parse sequence spec '%s'" % seq_info)
            self.seq = int(mat_seq.group(1))
            self.count = int(mat_seq.group(2))
        elif name == 'a/f/r/t':
            self.acked, self.fixes, self.reviewed, self.tested = to_int(
                value.split())
        elif name == 's/w/f':
            self.success, self.warning, self.fail = to_int(value)
        else:
            print('Unknown field: %s\n' % name)

    def SetUrl(self, url):
        """Sets the URL for a patch

        Args:
            url: URL to set
        """
        self.url = url


def FindResponses(url):
    """Collect the response tags (rtags) to a patch found in patchwork

    Reads the URL and checks each comment for an rtags (e.g. Reviewed-by).
    This does not look at the original patch (which have have rtags picked
    up from previous rounds of reviews). It only looks at comments which add
    new rtags.

    Args:
        url: URL of the patch

    Returns:
        Dict of rtags:
            key: rtag type, e.g. 'Acked-by'
            value: Set of people providing that rtag. Each member of the set
                is a name/email, e.g. 'Bin Meng <bmeng.cn@gmail.com>'
    """
    web_data = requests.get(url)
    if web_data.status_code != 200:
        raise ValueError("Could not read URL '%s'" % url)
    soup = BeautifulSoup(web_data.text, 'html.parser')

    # The first comment is the patch itself, so skip that
    comments = soup.findAll(class_='comment')[1:]
    rtags = collections.defaultdict(set)
    reviews = []

    if comments:
        for comment in comments:
            ps = patchstream.PatchStream(None)
            ps.commit = commit.Commit(None)
            meta = comment.find(class_='meta')
            content = comment.find(class_='content')
            infd = io.StringIO(content.get_text())
            outfd = io.StringIO()
            ps.ProcessStream(infd, outfd)
            for response, people in ps.commit.rtags.items():
                rtags[response].update(people)
    return rtags

def CollectPatches(series, url):
    """Collect patch information about a series from patchwork

    Reads the web page and collects information provided by patchwork about
    the status of each patch.

    Args:
        series: Series object corresponding to the local branch containing the
            series
        url: URL pointing to patchwork's view of this series, possibly an
            earlier versions of it

    Returns:
        List of patches sorted by sequence number, each a Patch object
    """
    response = requests.get(url)
    if response.status_code != 200:
        raise ValueError("Could not read URL '%s'" % url)
    soup = BeautifulSoup(response.text, 'html.parser')

    # Find the main table
    tables = soup.findAll('table')
    if len(tables) != 1:
        raise ValueError('Failed to parse page (expected only one table)')
    tab = tables[0]

    # Get all the rows, which are patches
    rows = tab.tbody.findAll('tr')
    count = len(rows)
    num_commits = len(series.commits)
    if count != num_commits:
        print('Warning: Web page reports %d patches, series has %d' %
              (count, num_commits))

    # Get the column headers, which we use to figure out which column is which
    hdrs = tab.thead.findAll('th')
    cols = []
    for hdr in hdrs:
        cols.append(hdr.get_text().strip())
    patches = []

    # Work through each row (patch) one at a time, collecting the information
    for row in rows:
        patch = Patch()
        for seq, col in enumerate(row.findAll('td')):
            name = cols[seq].lower()
            patch.Add(name, col.get_text())
            if name == 'patch':
                patch.SetUrl(urljoin(url, col.a['href']))
        if patch.count != count:
            raise ValueError("Patch %d '%s' has count of %d, expected %d" %
                (patch.seq, patch.subject, patch.count, num_commits))
        patches.append(patch)

    # Sort patches by patch number
    patches = sorted(patches, key=lambda x: x.seq)
    return patches

def FindNewResponses(new_rtag_list, seq, commit, patch):
    """Find new rtags collected by patchwork that we don't know about

    This is designed to be run in parallel, once for each commit/patch

    Args:
        new_rtag_list: New rtags are written to new_rtag_list[seq]
            list, each a dict:
                key: Response tag (e.g. 'Reviewed-by')
                value: Set of people who gave that response, each a name/email
                    string
        seq: Position in new_rtag_list to update
        commit: Commit object for this commit
        patch: Corresponding Patch object for this patch
    """
    acked = commit.rtags['Acked-by']
    reviewed = commit.rtags['Reviewed-by']
    tested = commit.rtags['Tested-by']
    fixes = commit.rtags['Fixes']

    # Figure out the number of new rtags by subtracting what we have in
    # the commit from what patchwork's summary reports.
    extra = 0
    extra += patch.acked - len(acked)
    extra += patch.fixes - len(fixes)
    extra += patch.reviewed - len(reviewed)
    extra += patch.tested - len(tested)

    # If patchwork agrees with the commit, we don't need to read the web
    # page, so save some time. If not, we need to add the new ones to the commit
    new_rtags = collections.defaultdict(set)
    if extra:
        pw_rtags = FindResponses(patch.url)
        base_rtags = commit.rtags
        new_count = 0
        for tag, people in pw_rtags.items():
            for who in people:
                is_new = (tag not in base_rtags or
                          who not in base_rtags[tag])
                if is_new:
                    new_rtags[tag].add(who)
                    new_count += 1
    new_rtag_list[seq] = new_rtags

def ShowResponses(rtags, indent, is_new):
    """Show rtags collected

    Args:
        rtags: dict:
            key: Response tag (e.g. 'Reviewed-by')
            value: Set of people who gave that response, each a name/email string
        indent: Indentation string to write before each line
        is_new: True if this output should be highlighted
    """
    col = terminal.Color()
    count = 0
    for tag, people in rtags.items():
        for who in people:
            terminal.Print(indent + '%s %s: ' % ('+' if is_new else ' ', tag),
                           newline=False, colour=col.GREEN, bright=is_new)
            terminal.Print(who, colour=col.WHITE, bright=is_new)
            count += 1
    return count

def CreateBranch(series, new_rtag_list, branch, dest_branch, overwrite):
    if branch == dest_branch:
        raise ValueError(
            'Destination branch must not be the same as the original branch')
    repo = pygit2.Repository('.')
    count = len(series.commits)
    old_br = repo.branches[branch]
    new_br = repo.branches.get(dest_branch)
    if new_br:
        if not overwrite:
            raise ValueError("Branch '%s' already exists (-f to overwrite)" %
                             dest_branch)
        new_br.delete()
    target = repo.revparse_single('%s~%d' % (branch, count))
    repo.branches.local.create(dest_branch, target)

    num_added = 0
    for seq in range(count):
        basket = repo.branches.get(dest_branch)
        cherry = repo.revparse_single('%s~%d' % (branch, count - seq - 1))

        base = repo.merge_base(cherry.oid, basket.target)
        base_tree = cherry.parents[0].tree

        index = repo.merge_trees(base_tree, basket, cherry)
        tree_id = index.write_tree(repo)

        author    = cherry.author
        committer = cherry.committer
        lines = []
        for tag, people in new_rtag_list[seq].items():
            for who in people:
                lines.append('%s: %s' % (tag, who))
                num_added += 1
        message = cherry.message + '\n'.join(lines)

        basket = repo.create_commit(
            basket.name, cherry.author, cherry.committer, message, tree_id,
            [basket.target])
    return num_added

def check_patchwork_status(series, url, branch, dest_branch, force):
    patches = CollectPatches(series, url)
    col = terminal.Color()
    count = len(patches)
    new_rtag_list = [None] * count

    with concurrent.futures.ThreadPoolExecutor(max_workers=16) as executor:
        futures = executor.map(
            FindNewResponses, repeat(new_rtag_list), range(count),
            series.commits, patches)
    for fresponse in futures:
        if fresponse:
            raise fresponse.exception()

    num_to_add = 0
    for seq, patch in enumerate(patches):
        terminal.Print('%3d %s' % (patch.seq, patch.subject[:50]),
                       colour=col.BLUE)
        commit = series.commits[seq]
        base_rtags = commit.rtags
        new_rtags = new_rtag_list[seq]

        indent = ' ' * 2
        ShowResponses(base_rtags, indent, False)
        num_to_add += ShowResponses(new_rtags, indent, True)

    print("%d new response%s available in patchwork%s" %
          (num_to_add, 's' if num_to_add != 1 else '',
           '' if dest_branch else ' (use -d to write them to a new branch)'))

    if dest_branch:
        num_added = CreateBranch(series, new_rtag_list, branch,
                                 dest_branch, force)
        print("%d response%s added from patchwork into new branch '%s'" %
              (num_added, 's' if num_added != 1 else '', dest_branch))
