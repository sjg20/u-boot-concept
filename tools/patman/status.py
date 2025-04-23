# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2020 Google LLC
#
"""Talks to the patchwork service to figure out what patches have been reviewed
and commented on. Provides a way to display review tags and comments.
Allows creation of a new branch based on the old but with the review tags
collected from patchwork.
"""

import asyncio
import collections

import aiohttp
import pygit2

from u_boot_pylib import terminal
from u_boot_pylib import tout
from patman import patchstream
from patman.patchstream import PatchStream
from patman import patchwork

def to_int(vals):
    """Convert a list of strings into integers, using 0 if not an integer

    Args:
        vals (list): List of strings

    Returns:
        list: List of integers, one for each input string
    """
    out = [int(val) if val.isdigit() else 0 for val in vals]
    return out


def compare_with_series(series, patches):
    """Compare a list of patches with a series it came from

    This prints any problems as warnings

    Args:
        series (Series): Series to compare against
        patches (list of Patch): list of Patch objects to compare with

    Returns:
        tuple
            dict:
                key: Commit number (0...n-1)
                value: Patch object for that commit
            dict:
                key: Patch number  (0...n-1)
                value: Commit object for that patch
    """
    # Check the names match
    warnings = []
    patch_for_commit = {}
    all_patches = set(patches)
    for seq, cmt in enumerate(series.commits):
        pmatch = [p for p in all_patches if p.subject == cmt.subject]
        if len(pmatch) == 1:
            patch_for_commit[seq] = pmatch[0]
            all_patches.remove(pmatch[0])
        elif len(pmatch) > 1:
            warnings.append("Multiple patches match commit %d ('%s'):\n   %s" %
                            (seq + 1, cmt.subject,
                             '\n   '.join([p.subject for p in pmatch])))
        else:
            warnings.append("Cannot find patch for commit %d ('%s')" %
                            (seq + 1, cmt.subject))


    # Check the names match
    commit_for_patch = {}
    all_commits = set(series.commits)
    for seq, patch in enumerate(patches):
        cmatch = [c for c in all_commits if c.subject == patch.subject]
        if len(cmatch) == 1:
            commit_for_patch[seq] = cmatch[0]
            all_commits.remove(cmatch[0])
        elif len(cmatch) > 1:
            warnings.append("Multiple commits match patch %d ('%s'):\n   %s" %
                            (seq + 1, patch.subject,
                             '\n   '.join([c.subject for c in cmatch])))
        else:
            warnings.append("Cannot find commit for patch %d ('%s')" %
                            (seq + 1, patch.subject))

    return patch_for_commit, commit_for_patch, warnings


async def _collect_patches(client, expect_count, series_id, pwork,
                           read_comments, read_cover_comments):
    """Collect patch information about a series from patchwork

    Uses the Patchwork REST API to collect information provided by patchwork
    about the status of each patch.

    Args:
        client (aiohttp.ClientSession): Session to use
        expect_count (int): Number of patches expected
        series_id (str): Patch series ID number
        pwork (Patchwork): Patchwork class to handle communications
        read_comments (bool): True to read the comments on the patches
        read_cover_comments (bool): True to read the comments on the cover
            letter

    Returns:
        list: List of patches sorted by sequence number, each a Patch object

    Raises:
        ValueError: if the URL could not be read or the web page does not follow
            the expected structure
    """
    cover, patch_list = await pwork._series_get_state(
        client, series_id, read_comments, read_cover_comments)

    # Get all the rows, which are patches
    count = len(patch_list)
    if count != expect_count:
        tout.warning(f'Warning: Patchwork reports {count} patches, series has '
                     f'{expect_count}')

    return cover, patch_list


def process_reviews(content, comment_data, base_rtags):
    """Process and return review data

    Args:
        content (str): Content text of the patch itself - see pwork.get_patch()
        comment_data (list of dict): Comments for the patch - see
            pwork._get_patch_comments()
        base_rtags (dict): base review tags (before any comments)
            key: Response tag (e.g. 'Reviewed-by')
            value: Set of people who gave that response, each a name/email
                string

    Return: tuple:
        dict: new review tags (noticed since the base_rtags)
            key: Response tag (e.g. 'Reviewed-by')
            value: Set of people who gave that response, each a name/email
                string
        list of patchwork.Review: reviews received on the patch
    """
    pstrm = PatchStream.process_text(content, True)
    rtags = collections.defaultdict(set)
    for response, people in pstrm.commit.rtags.items():
        rtags[response].update(people)

    reviews = []
    for comment in comment_data:
        pstrm = PatchStream.process_text(comment['content'], True)
        if pstrm.snippets:
            submitter = comment['submitter']
            person = f"{submitter['name']} <{submitter['email']}>"
            reviews.append(patchwork.Review(person, pstrm.snippets))
        for response, people in pstrm.commit.rtags.items():
            rtags[response].update(people)

    # Find the tags that are not in the commit
    new_rtags = collections.defaultdict(set)
    for tag, people in rtags.items():
        for who in people:
            is_new = (tag not in base_rtags or
                      who not in base_rtags[tag])
            if is_new:
                new_rtags[tag].add(who)
    return new_rtags, reviews


def show_responses(col, rtags, indent, is_new):
    """Show rtags collected

    Args:
        col (terminal.Colour): Colour object to use
        rtags (dict): review tags to show
            key: Response tag (e.g. 'Reviewed-by')
            value: Set of people who gave that response, each a name/email string
        indent (str): Indentation string to write before each line
        is_new (bool): True if this output should be highlighted

    Returns:
        int: Number of review tags displayed
    """
    count = 0
    for tag in sorted(rtags.keys()):
        people = rtags[tag]
        for who in sorted(people):
            terminal.tprint(indent + '%s %s: ' % ('+' if is_new else ' ', tag),
                           newline=False, colour=col.GREEN, bright=is_new,
                           col=col)
            terminal.tprint(who, colour=col.WHITE, bright=is_new, col=col)
            count += 1
    return count

def create_branch(series, new_rtag_list, branch, dest_branch, overwrite,
                  repo=None):
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
        repo (pygit2.Repository): Repo to use (use None unless testing)

    Returns:
        int: Total number of review tags added across all commits

    Raises:
        ValueError: if the destination branch name is the same as the original
            branch, or it already exists and @overwrite is False
    """
    if branch == dest_branch:
        raise ValueError(
            'Destination branch must not be the same as the original branch')
    if not repo:
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
        if new_rtag_list[seq]:
            for tag, people in new_rtag_list[seq].items():
                for who in people:
                    lines.append('%s: %s' % (tag, who))
                    num_added += 1
        message = patchstream.insert_tags(cherry.message.rstrip(),
                                          sorted(lines))

        repo.create_commit(
            parent.name, cherry.author, cherry.committer, message, tree_id,
            [parent.target])
    return num_added


async def _check_status(client, series, series_id, branch, dest_branch, force,
                        show_comments, show_cover_comments, pwork,
                        test_repo=None):
    """Check the status of a series on Patchwork

    This finds review tags and comments for a series in Patchwork, displaying
    them to show what is new compared to the local series.

    Args:
        client (aiohttp.ClientSession): Session to use
        series (Series): Series object for the existing branch
        series_id (str): Patch series ID number
        branch (str): Existing branch to update, or None
        dest_branch (str): Name of new branch to create, or None
        force (bool): True to force overwriting dest_branch if it exists
        show_comments (bool): True to show the comments on each patch
        show_cover_comments (bool): True to show the comments on the
            letter
        pwork (Patchwork): Patchwork class to handle communications
        test_repo (pygit2.Repository): Repo to use (use None unless testing)
    """
    cover, patches = await _collect_patches(client, len(series.commits),
                                            series_id, pwork, True,
                                            show_cover_comments)

    compare = []
    for pw_patch in patches:
        patch = patchwork.Patchx(pw_patch.id)
        patch.parse_subject(pw_patch.series_data['name'])
        compare.append(patch)

    col = terminal.Color()
    count = len(series.commits)
    new_rtag_list = [None] * count
    review_list = [None] * count

    patch_for_commit, _, warnings = compare_with_series(series, compare)
    for warn in warnings:
        tout.warning(warn)

    for seq, pw_patch in enumerate(patches):
        compare[seq].patch = pw_patch

    for i in range(count):
        pat = patch_for_commit.get(i)
        if pat:
            patch_data = pat.patch.data
            comment_data = pat.patch.comments
            new_rtag_list[i], review_list[i] = process_reviews(
                patch_data['content'], comment_data, series.commits[i].rtags)

    with terminal.pager():
        num_to_add = 0

        if cover:
            terminal.tprint(f'Cov {cover.name}', colour=col.BLACK, col=col,
                            bright=False, back=col.YELLOW)
            for seq, comment in enumerate(cover.comments):
                submitter = comment['submitter']
                person = '%s <%s>' % (submitter['name'], submitter['email'])
                terminal.tprint(f'From: {person}: {comment['date']}',
                                colour=col.RED, col=col)
                print(comment['content'])
                print()

        for seq, cmt in enumerate(series.commits):
            patch = patch_for_commit.get(seq)
            if not patch:
                continue
            terminal.tprint('%3d %s' % (patch.seq, patch.subject[:50]),
                           colour=col.YELLOW, col=col)
            cmt = series.commits[seq]
            base_rtags = cmt.rtags
            new_rtags = new_rtag_list[seq]

            indent = ' ' * 2
            show_responses(col, base_rtags, indent, False)
            num_to_add += show_responses(col, new_rtags, indent, True)
            if show_comments:
                for review in review_list[seq]:
                    terminal.tprint('Review: %s' % review.meta, colour=col.RED,
                                    col=col)
                    for snippet in review.snippets:
                        for line in snippet:
                            quoted = line.startswith('>')
                            terminal.tprint(
                                f'    {line}',
                                colour=col.MAGENTA if quoted else None, col=col)
                        terminal.tprint()

        terminal.tprint("%d new response%s available in patchwork%s" %
                       (num_to_add, 's' if num_to_add != 1 else '',
                        '' if dest_branch or not num_to_add
                        else ' (use -d to write them to a new branch)'))

        if dest_branch:
            num_added = create_branch(series, new_rtag_list, branch,
                                      dest_branch, force, test_repo)
            terminal.tprint(
                "%d response%s added from patchwork into new branch '%s'" %
                (num_added, 's' if num_added != 1 else '', dest_branch))


async def check_status(series, series_id, branch, dest_branch, force,
                       show_comments, show_cover_comments, patchwork,
                       test_repo=None):
    async with aiohttp.ClientSession() as client:
        await _check_status(
            client, series, series_id, branch, dest_branch,  force,
            show_comments, show_cover_comments, patchwork, test_repo=test_repo)


def check_patchwork_status(series, series_id, branch, dest_branch, force,
                           show_comments, show_cover_comments, patchwork,
                           test_repo=None, single_thread=False):
    if single_thread:
        asyncio.run(check_status(series, series_id, branch, dest_branch, force,
                                 show_comments, show_cover_comments, patchwork,
                                 test_repo=test_repo))
    else:
        loop = asyncio.get_event_loop()
        loop.run_until_complete(check_status(
                series, series_id, branch, dest_branch,  force, show_comments,
                show_cover_comments, patchwork, test_repo=test_repo))
