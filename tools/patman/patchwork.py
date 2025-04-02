# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2025 Simon Glass <sjg@chromium.org>
#
"""Provides a basic API for the patchwork server
"""

from collections import namedtuple
import requests

# Information about a patch on patchwork
# id: Patchwork ID of patch
# state: Current state, e.g. 'accepted'
PATCH = namedtuple('patch', 'id,state')


class Patchwork:
    """Class to handle communication with patchwork
    """
    def __init__(self, url):
        """Set up a new patchwork handler

        Args:
            url (str): URL of patchwork server, e.g. 'https://patchwork.ozlabs.org'
        """
        self.url = url
        self.fake_request = None
        self.proj_id = None

    def request(self, subpath):
        """Call the patchwork API and return the result as JSON

        Args:
            url (str): URL of patchwork server, e.g. 'https://patchwork.ozlabs.org'
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
        pwork = Patchwork(None)
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

    def set_project(self, project_id):
        """Set the project ID

        The patchwork server has multiple projects. This allows the ID of the
        relevant project to be selected

        Args:
            project_id (int): Project ID to use
        """
        self.proj_id = project_id

    def get_series(self, series_id):
        """Read information about a series

        Args:
            series_id (str): Patchwork series ID

        Returns:
            dict containing patchwork's series information
        """
        return self.request(f'series/{series_id}/')

    def series_get_state(self, series_id):
        """Sync the series information against patwork, to find patch status

        Args:
            series_id (str): Patchwork series ID

        Return:
            list of PATCH: patch information for each patch in the series
        """
        data = self.get_series(series_id)
        patch_dict = data['patches']

        result = []
        for pat in patch_dict:
            patch_id = pat['id']
            data = self.request('patches/%s/' % patch_id)
            result.append(PATCH(patch_id, data['state']))
        return result
