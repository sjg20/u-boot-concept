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

import aiohttp
import pygit2

from u_boot_pylib import terminal
from u_boot_pylib import tout
from patman import patchstream
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
    with terminal.pager():
        num_to_add, new_rtag_list, _, _ = await pwork._check_status(
            client, series, series_id, branch, show_comments,
            show_cover_comments)

        if not dest_branch and num_to_add:
            msg = ' (use -d to write them to a new branch)'
        else:
            msg = ''
        terminal.tprint(
            f"{num_to_add} new response{'s' if num_to_add != 1 else ''} "
            f'available in patchwork{msg}')

        if dest_branch:
            num_added = create_branch(series, new_rtag_list, branch,
                                      dest_branch, force, test_repo)
            terminal.tprint(
                f"{num_added} response{'s' if num_added != 1 else ''} added "
                f"from patchwork into new branch '{dest_branch}'")


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
