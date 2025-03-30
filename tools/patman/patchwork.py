# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2025 Simon Glass <sjg@chromium.org>
#
"""Provides a basic API for the patchwork server
"""

import requests

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

    def find_series(self, name):
        query = name.replace(' ', '+')
        res = self.request(f'series/?project={self.proj_id}&q={query}')
        for ser in res:
            if ser['name'] == name:
                return ser['id'], None
        return none, res

    def set_project(self, project_id):
        self.proj_id = project_id
