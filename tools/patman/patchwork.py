# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2025 Simon Glass <sjg@chromium.org>
#
"""Provides a basic API for the patchwork server
"""

from collections import namedtuple
import concurrent
from itertools import repeat
import requests

from u_boot_pylib import terminal

# Information about a patch on patchwork
# id: Patchwork ID of patch
# state: Current state, e.g. 'accepted'
PATCH = namedtuple('patch', 'id,state,num_comments')


class Patchwork:
    """Class to handle communication with patchwork
    """
    def __init__(self, url, show_progress=True):
        """Set up a new patchwork handler

        Args:
            url (str): URL of patchwork server, e.g. 'https://patchwork.ozlabs.org'
        """
        self.url = url
        self.fake_request = None
        self.proj_id = None
        self.link_name = None
        self._show_progress = show_progress

    def request(self, subpath):
        """Call the patchwork API and return the result as JSON

        Args:
            subpath (str): URL subpath to use

        Returns:
            dict: Json result

        Raises:
            ValueError: the URL could not be read
        """
        if self.fake_request:
            return self.fake_request(subpath)

        full_url = '%s/api/1.2/%s' % (self.url, subpath)
        response = requests.get(full_url)
        if response.status_code != 200:
            raise ValueError("Could not read URL '%s'" % full_url)
        return response.json()

    @staticmethod
    def for_testing(func):
        pwork = Patchwork(None, show_progress=False)
        pwork.fake_request = func
        return pwork

    def find_series(self, desc, version):
        """Find a series on the server

        Args:
            desc (str): Description to search for
            version (int): Version number to search for

        Returns:
            tuple:
                str: Series ID, or None if not found
                list of dict, or None if found
                    each dict is the server result from a possible series
        """
        query = desc.replace(' ', '+')
        res = self.request(f'series/?project={self.proj_id}&q={query}')
        name_found = []
        for ser in res:
            if ser['name'] == desc:
                if int(ser['version']) == version:
                    return ser['id'], None
                name_found.append(ser)
        return None, name_found or res

    def set_project(self, project_id, link_name):
        """Set the project ID

        The patchwork server has multiple projects. This allows the ID and
        link_name of the relevant project to be selected

        This function is used for testing

        Args:
            project_id (int): Project ID to use, e.g. 6
            link_name (str): Name to use for project URL links, e.g. 'uboot'
        """
        self.proj_id = project_id
        self.link_name = link_name

    def get_series(self, series_id):
        """Read information about a series

        Args:
            series_id (str): Patchwork series ID

        Returns:
            dict containing patchwork's series information
        """
        return self.request(f'series/{series_id}/')

    def get_series_url(self, series_id):
        """Get the URL for a series

        Args:
            series_id (str): Patchwork series ID

        Returns:
            str: URL for the series page
        """
        return f'{self.url}/project/{self.link_name}/list/?series={series_id}&state=*&archive=both'

    def _get_patch_status(self, patch_dict, seq, result, count):
        patch_id = patch_dict[seq]['id']
        data = self.request(f'patches/{patch_id}/')
        state = data['state']
        data = self.request(f'patches/{patch_id}/comments/')
        num_comments = len(data)

        result[seq] = PATCH(patch_id, state, num_comments)
        done = len([1 for i in range(len(result)) if result[i]])

        if self._show_progress:
            terminal.tprint(f'\r{count - done}  ', newline=False)

    def series_get_state(self, series_id):
        """Sync the series information against patwork, to find patch status

        Args:
            series_id (str): Patchwork series ID

        Return:
            list of PATCH: patch information for each patch in the series
        """
        data = self.get_series(series_id)
        patch_dict = data['patches']

        count = len(patch_dict)
        result = [None] * count
        with concurrent.futures.ThreadPoolExecutor(max_workers=16) as executor:
            futures = executor.map(
                self._get_patch_status, repeat(patch_dict), range(count),
                repeat(result), repeat(count))
        for fresponse in futures:
            if fresponse:
                raise fresponse.exception()
        if self._show_progress:
            terminal.print_clear()

        return result
