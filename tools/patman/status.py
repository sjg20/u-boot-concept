# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2020 Google LLC
#

"""Talks to the patchwork service to figure out what patches have been
reviewed and commented on. Provides a way to display review tags and comments.
Allows creation of a new branch based on the old but with the review tags
collected from patchwork.
"""

import collections
import concurrent.futures
import io
from itertools import repeat
import re
from urllib.parse import urljoin
import pygit2
import requests
from bs4 import BeautifulSoup

from patman import commit
from patman import patchstream
from patman import terminal

# Patches which are part of a multi-patch series are shown with a prefix like
# [prefix, version, sequence], for example '[RFC, v2, 3/5]'. All but the last
# part is optional. This decodes the string into groups. For single patches
# the [] part is not present:
# Groups: (ignore, ignore, ignore, prefix, version, sequence, subject)
RE_PATCH = re.compile(r'(\[(((.*),)?(.*),)?(.*)\]\s)?(.*)$')

# This decodes the sequence string into a patch number and patch count
RE_SEQ = re.compile(r'(\d+)/(\d+)')

def to_int(vals):
    """Convert a list of strings into integers, using 0 if not an integer

    Args:
        vals (list): List of strings

    Returns:
        list: List of integers, one for each input string
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
        seq (int): Sequence number within series (1=first) parsed from sequence
            string
        count (int): Number of patches in series, parsed from sequence string
        prefix (str): Prefix string or None (e.g. 'RFC')
        version (str): Version string or None (e.g. 'v2')
        subject (str): Patch subject with [..] part removed (same as commit
            subject)
        series (str): Series name
        acked (int): Number of Acked-by rtags, or None if not known
        fixes (int): Number of Fixes rtags, or None if not known
        reviewed (int): Number of Reviewed-by rtags, or None if not known
        tested (int): Number of Tested-by rtags, or None if not known
        success (int): Success value, or None if not known
        warning (int): Warning value, or None if not known
        fail (int): Failure value, or None if not known
        date (str): Date as a string
        Submitter (str): Submitter name
        Delegate (str): Delegate ID
        state (str): Current state (e.g. 'New')
        url (str): URL pointing to the patch on patchwork
    """
    def __init__(self):
        super().__init__()
        self.seq = None
        self.count = None
        self.prefix = None
        self.version = None
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

    def add(self, name, value):
        """Add information obtained from the web page

        This sets the object properties based on @name.

        Args:
            name (str): Column name in lower-case, e.g. 'Series' or 'State'
            value (str): Text obtained for this patch from that column,
                 e.g. 'New'

        Raises:
            ValueError: if the name cannot be parsed or is not recognised
        """
        if name in self:
            self[name] = value
        elif name == 'patch':
            mat = RE_PATCH.search(value.strip())
            if not mat:
                raise ValueError("Cannot parse subject '%s'" % value)
            self.prefix = mat.group(1)
            self.prefix, self.version, seq_info, self.subject = mat.groups()[3:]
            mat_seq = RE_SEQ.match(seq_info)
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

    def set_url(self, url):
        """Sets the URL for a patch

        Args:
            url (str): URL to set
        """
        self.url = url


class Review:
    """Represents a single review email collected in Patchwork

    Patches can attract multiple reviews. Each consists of an author/date and
    a variable number of 'snippets', which are groups of quoted and unquoted
    text.
    """
    def __init__(self, meta, snippets):
        """Create new Review object

        Args:
            meta (str): Text containing review author and date
            snippets (list): List of snippets in th review, each a list of text
                lines
        """
        self.meta = ' : '.join([line for line in meta.splitlines() if line])
        self.snippets = snippets


def find_responses(url):
    """Collect the response tags (rtags) to a patch found in patchwork

    Reads the URL and checks each comment for an rtags (e.g. Reviewed-by).
    This does not look at the original patch (which have have rtags picked
    up from previous rounds of reviews). It only looks at comments which add
    new rtags.

    Args:
        url (str): URL of the patch

    Returns:
        Tuple:
            Dict of rtags:
                key: rtag type, e.g. 'Acked-by'
                value: Set of people providing that rtag. Each member of the set
                    is a name/email, e.g. 'Bin Meng <bmeng.cn@gmail.com>'
            List of Review objects for the patch

    Raises:
        ValueError: if the URL could not be read
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
            pstrm = patchstream.PatchStream(None)
            pstrm.commit = commit.Commit(None)
            meta = comment.find(class_='meta')
            content = comment.find(class_='content')
            infd = io.StringIO(content.get_text())
            outfd = io.StringIO()
            pstrm.ProcessStream(infd, outfd)
            for response, people in pstrm.commit.rtags.items():
                rtags[response].update(people)
            if pstrm.snippets:
                reviews.append(Review(meta.get_text(), pstrm.snippets))

    return rtags, reviews

def collect_patches(series, url):
    """Collect patch information about a series from patchwork

    Reads the web page and collects information provided by patchwork about
    the status of each patch.

    Args:
        series (Series): Series object corresponding to the local branch
            containing the series
        url (str): URL pointing to patchwork's view of this series, possibly an
            earlier versions of it

    Returns:
        list: List of patches sorted by sequence number, each a Patch object

    Raises:
        ValueError: if the URL could not be read or the web page does not follow
            the expected structure
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
            patch.add(name, col.get_text())
            if name == 'patch':
                patch.set_url(urljoin(url, col.a['href']))
        if patch.count != count:
            raise ValueError(
                "Patch %d '%s' has count of %d, expected %d" %
                (patch.seq, patch.subject, patch.count, num_commits))
        patches.append(patch)

    # Sort patches by patch number
    patches = sorted(patches, key=lambda x: x.seq)
    return patches

def find_new_responses(new_rtag_list, review_list, seq, cmt, patch,
                       force_load):
    """Find new rtags collected by patchwork that we don't know about

    This is designed to be run in parallel, once for each commit/patch

    Args:
        new_rtag_list (list): New rtags are written to new_rtag_list[seq]
            list, each a dict:
                key: Response tag (e.g. 'Reviewed-by')
                value: Set of people who gave that response, each a name/email
                    string
        review_list (list): New reviews are written to review_list[seq]
            list, each a
                List of reviews for the patch, each a Review
        seq (int): Position in new_rtag_list to update
        cmt (Commit): Commit object for this commit
        patch (Patch): Corresponding Patch object for this patch
        force_load (bool): True to always load the patch from patchwork, False
            to only load it if patchwork has additional response tags
    """
    acked = cmt.rtags['Acked-by']
    reviewed = cmt.rtags['Reviewed-by']
    tested = cmt.rtags['Tested-by']
    fixes = cmt.rtags['Fixes']

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
    reviews = None
    if extra or force_load:
        pw_rtags, reviews = find_responses(patch.url)
        base_rtags = cmt.rtags
        new_count = 0
        for tag, people in pw_rtags.items():
            for who in people:
                is_new = (tag not in base_rtags or
                          who not in base_rtags[tag])
                if is_new:
                    new_rtags[tag].add(who)
                    new_count += 1
    new_rtag_list[seq] = new_rtags
    review_list[seq] = reviews

def show_responses(rtags, indent, is_new):
    """Show rtags collected

    Args:
        rtags (dict): review tags to show
            key: Response tag (e.g. 'Reviewed-by')
            value: Set of people who gave that response, each a name/email string
        indent (str): Indentation string to write before each line
        is_new (bool): True if this output should be highlighted

    Returns:
        int: Number of review tags displayed
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

def create_branch(series, new_rtag_list, branch, dest_branch, overwrite):
    """Create a new branch with review tags added

    Args:
        series (Series): Series object for the existing branch
        new_rtag_list (list): List of review tags to add, one for each commit,
                each a dict:
            key: Response tag (e.g. 'Reviewed-by')
            value: Set of people who gave that response, each a name/email
                string
        branch (str): Existing branch to update
        dest_branch (str): Name of new branch to create
        overwrite (bool): True to force overwriting dest_branch if it exists

    Returns:
        int: Total number of review tags added across all commits

    Raises:
        ValueError: if the destination branch name is the same as the original
            branch, or it already exists and @overwrite is False
    """
    if branch == dest_branch:
        raise ValueError(
            'Destination branch must not be the same as the original branch')
    repo = pygit2.Repository('.')
    count = len(series.commits)
    new_br = repo.branches.get(dest_branch)
    if new_br:
        if not overwrite:
            raise ValueError("Branch '%s' already exists (-f to overwrite)" %
                             dest_branch)
        new_br.delete()
    if not branch:
        branch = 'HEAD'
    target = repo.revparse_single('%s~%d' % (branch, count))
    repo.branches.local.create(dest_branch, target)

    num_added = 0
    for seq in range(count):
        parent = repo.branches.get(dest_branch)
        cherry = repo.revparse_single('%s~%d' % (branch, count - seq - 1))

        repo.merge_base(cherry.oid, parent.target)
        base_tree = cherry.parents[0].tree

        index = repo.merge_trees(base_tree, parent, cherry)
        tree_id = index.write_tree(repo)

        lines = []
        for tag, people in new_rtag_list[seq].items():
            for who in people:
                lines.append('%s: %s' % (tag, who))
                num_added += 1
        message = cherry.message.rstrip() + '\n' + '\n'.join(lines)

        repo.create_commit(
            parent.name, cherry.author, cherry.committer, message, tree_id,
            [parent.target])
    return num_added

def check_patchwork_status(series, url, branch, dest_branch, force,
                           show_comments):
    """Check the status of a series on Patchwork

    This finds review tags and comments for a series in Patchwork, displaying
    them and optionally creating a new branch like the old but with the new
    review tags.

    Args:
        series (Series): Series object for the existing branch
        url (str): URL pointing to the patch on patchwork
        branch (str): Existing branch to update
        dest_branch (str): Name of new branch to create
        force (bool): True to force overwriting dest_branch if it exists
        show_comments (bool): True to show the comments on each patch
    """
    patches = collect_patches(series, url)
    col = terminal.Color()
    count = len(patches)
    new_rtag_list = [None] * count
    review_list = [None] * count

    with concurrent.futures.ThreadPoolExecutor(max_workers=16) as executor:
        futures = executor.map(
            find_new_responses, repeat(new_rtag_list), repeat(review_list),
            range(count), series.commits, patches, repeat(show_comments))
    for fresponse in futures:
        if fresponse:
            raise fresponse.exception()

    num_to_add = 0
    for seq, patch in enumerate(patches):
        terminal.Print('%3d %s' % (patch.seq, patch.subject[:50]),
                       colour=col.BLUE)
        cmt = series.commits[seq]
        base_rtags = cmt.rtags
        new_rtags = new_rtag_list[seq]

        indent = ' ' * 2
        show_responses(base_rtags, indent, False)
        num_to_add += show_responses(new_rtags, indent, True)
        if show_comments:
            for review in review_list[seq]:
                terminal.Print('Review: %s' % review.meta, colour=col.RED)
                for snippet in review.snippets:
                    for line in snippet:
                        quoted = line.startswith('>')
                        terminal.Print('    %s' % line,
                                       colour=col.MAGENTA if quoted else None)
                    print()

    print("%d new response%s available in patchwork%s" %
          (num_to_add, 's' if num_to_add != 1 else '',
           '' if dest_branch else ' (use -d to write them to a new branch)'))

    if dest_branch:
        num_added = create_branch(series, new_rtag_list, branch,
                                  dest_branch, force)
        print("%d response%s added from patchwork into new branch '%s'" %
              (num_added, 's' if num_added != 1 else '', dest_branch))
