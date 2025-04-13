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
# id (int): Patchwork ID of patch
# state (str): Current state, e.g. 'accepted'
# num_comments (int): Number of comments
PATCH = namedtuple('patch', 'id,state,num_comments')

# Information about a cover-letter on patchwork
# id (int): Patchwork ID of cover letter
# state (str): Current state, e.g. 'accepted'
# num_comments (int): Number of comments
# name (str): Series name
COVER = namedtuple('cover', 'id,num_comments,name')


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

        Returns: dict containing patchwork's series information
            id (int): series ID unique across patchwork instance, e.g. 3
            url (str): Full URL, e.g. 'https://patchwork.ozlabs.org/api/1.2/series/3/'
            web_url (str): Full URL, e.g. 'https://patchwork.ozlabs.org/project/uboot/list/?series=3
            project (dict): project information (id, url, name, link_name,
                list_id, list_email, etc.
            name (str): Series name, e.g. '[U-Boot] moveconfig: fix error'
            date (str): Date, e.g. '2017-08-27T08:00:51'
            submitter (dict): id, url, name, email, e.g.:
                "id": 6125,
                "url": "https://patchwork.ozlabs.org/api/1.2/people/6125/",
                "name": "Chris Packham",
                "email": "judge.packham@gmail.com"
            version (int): Version number
            total (int): Total number of patches based on subject
            received_total (int): Total patches received by patchwork
            received_all (bool): True if all patches were received
            mbox (str): URL of mailbox, e.g. 'https://patchwork.ozlabs.org/series/3/mbox/'
            cover_letter (dict) or None, e.g.:
                "id": 806215,
                "url": "https://patchwork.ozlabs.org/api/1.2/covers/806215/",
                "web_url": "https://patchwork.ozlabs.org/project/uboot/cover/20170827094411.8583-1-judge.packham@gmail.com/",
                "msgid": "<20170827094411.8583-1-judge.packham@gmail.com>",
                "list_archive_url": null,
                "date": "2017-08-27T09:44:07",
                "name": "[U-Boot,v2,0/4] usb: net: Migrate USB Ethernet to Kconfig",
                "mbox": "https://patchwork.ozlabs.org/project/uboot/cover/20170827094411.8583-1-judge.packham@gmail.com/mbox/"
            patches (list of dict), each e.g.:
                "id": 806202,
                "url": "https://patchwork.ozlabs.org/api/1.2/patches/806202/",
                "web_url": "https://patchwork.ozlabs.org/project/uboot/patch/20170827080051.816-1-judge.packham@gmail.com/",
                "msgid": "<20170827080051.816-1-judge.packham@gmail.com>",
                "list_archive_url": null,
                "date": "2017-08-27T08:00:51",
                "name": "[U-Boot] moveconfig: fix error message in do_autoconf()",
                "mbox": "https://patchwork.ozlabs.org/project/uboot/patch/20170827080051.816-1-judge.packham@gmail.com/mbox/"
        """
        return self.request(f'series/{series_id}/')

    def get_patch(self, patch_id):
        """Read information about a patch

        Args:
            patch_id (str): Patchwork patch ID

        Returns:
            dict containing patchwork's patch information
        """
        return self.request(f'patches/{patch_id}/')

    def get_patch_comments(self, patch_id):
        """Read comments about a patch

        Args:
            patch_id (str): Patchwork patch ID

        Returns: list of dict: list of comments:
            id (int): series ID unique across patchwork instance, e.g. 3331924
            web_url (str): Full URL, e.g. 'https://patchwork.ozlabs.org/comment/3331924/'
            msgid (str): Message ID, e.g. '<d2526c98-8198-4b8b-ab10-20bda0151da1@gmx.de>'
            list_archive_url: (unknown?)
            date (str): Date, e.g. '2024-06-20T13:38:03'
            subject (str): email subject, e.g. 'Re: [PATCH 3/5] buildman: Support building within a Python venv'
            date (str): Date, e.g. '2017-08-27T08:00:51'
            submitter (dict): id, url, name, email, e.g.:
                "id": 61270,
                "url": "https://patchwork.ozlabs.org/api/people/61270/",
                "name": "Heinrich Schuchardt",
                "email": "xypron.glpk@gmx.de"
            content (str): Content of email, e.g. 'On 20.06.24 15:19, Simon Glass wrote:\n>...'
            headers: dict: email headers, see get_cover() for an example
        """
        return self.request(f'patches/{patch_id}/comments/')

    def get_cover(self, cover_id):
        """Read information about a cover letter

        Args:
            cover_id (int): Patchwork cover-letter ID

        Returns: dict containing patchwork's cover-letter information:
            id (int): series ID unique across patchwork instance, e.g. 3
            url (str): Full URL, e.g. https://patchwork.ozlabs.org/project/uboot/list/?series=3
            project (dict): project information (id, url, name, link_name,
                list_id, list_email, etc.
            url (str): Full URL, e.g. 'https://patchwork.ozlabs.org/api/1.2/covers/2054866/'
            web_url (str): Full URL, e.g. 'https://patchwork.ozlabs.org/project/uboot/cover/20250304130947.109799-1-sjg@chromium.org/'
            project (dict): project information (id, url, name, link_name,
                list_id, list_email, etc.
            msgid (str): Message ID, e.g. '20250304130947.109799-1-sjg@chromium.org>'
            list_archive_url (?)
            date (str): Date, e.g. '2017-08-27T08:00:51'
            name (str): Series name, e.g. '[U-Boot] moveconfig: fix error'
            submitter (dict): id, url, name, email, e.g.:
                "id": 6170,
                "url": "https://patchwork.ozlabs.org/api/1.2/people/6170/",
                "name": "Simon Glass",
                "email": "sjg@chromium.org"
            mbox (str): URL to mailbox, e.g. 'https://patchwork.ozlabs.org/project/uboot/cover/20250304130947.109799-1-sjg@chromium.org/mbox/'
            series (list of dict) each e.g.:
                "id": 446956,
                "url": "https://patchwork.ozlabs.org/api/1.2/series/446956/",
                "web_url": "https://patchwork.ozlabs.org/project/uboot/list/?series=446956",
                "date": "2025-03-04T13:09:37",
                "name": "binman: Check code-coverage requirements",
                "version": 1,
                "mbox": "https://patchwork.ozlabs.org/series/446956/mbox/"
            comments: Web URL to comments: 'https://patchwork.ozlabs.org/api/covers/2054866/comments/'
            headers: dict: e.g.:
                "Return-Path": "<u-boot-bounces@lists.denx.de>",
                "X-Original-To": "incoming@patchwork.ozlabs.org",
                "Delivered-To": "patchwork-incoming@legolas.ozlabs.org",
                "Authentication-Results": [
                    "legolas.ozlabs.org;\n\tdkim=pass (1024-bit key;\n unprotected) header.d=chromium.org header.i=@chromium.org header.a=rsa-sha256\n header.s=google header.b=dG8yqtoK;\n\tdkim-atps=neutral",
                    "legolas.ozlabs.org;\n spf=pass (sender SPF authorized) smtp.mailfrom=lists.denx.de\n (client-ip=85.214.62.61; helo=phobos.denx.de;\n envelope-from=u-boot-bounces@lists.denx.de; receiver=patchwork.ozlabs.org)",
                    "phobos.denx.de;\n dmarc=pass (p=none dis=none) header.from=chromium.org",
                    "phobos.denx.de;\n spf=pass smtp.mailfrom=u-boot-bounces@lists.denx.de",
                    "phobos.denx.de;\n\tdkim=pass (1024-bit key;\n unprotected) header.d=chromium.org header.i=@chromium.org\n header.b=\"dG8yqtoK\";\n\tdkim-atps=neutral",
                    "phobos.denx.de;\n dmarc=pass (p=none dis=none) header.from=chromium.org",
                    "phobos.denx.de;\n spf=pass smtp.mailfrom=sjg@chromium.org"
                ],
                "Received": [
                    "from phobos.denx.de (phobos.denx.de [85.214.62.61])\n\t(using TLSv1.3 with cipher TLS_AES_256_GCM_SHA384 (256/256 bits)\n\t key-exchange X25519 server-signature ECDSA (secp384r1))\n\t(No client certificate requested)\n\tby legolas.ozlabs.org (Postfix) with ESMTPS id 4Z6bd50jLhz1yD0\n\tfor <incoming@patchwork.ozlabs.org>; Wed,  5 Mar 2025 00:10:00 +1100 (AEDT)",
                    "from h2850616.stratoserver.net (localhost [IPv6:::1])\n\tby phobos.denx.de (Postfix) with ESMTP id 434E88144A;\n\tTue,  4 Mar 2025 14:09:58 +0100 (CET)",
                    "by phobos.denx.de (Postfix, from userid 109)\n id 8CBF98144A; Tue,  4 Mar 2025 14:09:57 +0100 (CET)",
                    "from mail-io1-xd2e.google.com (mail-io1-xd2e.google.com\n [IPv6:2607:f8b0:4864:20::d2e])\n (using TLSv1.3 with cipher TLS_AES_128_GCM_SHA256 (128/128 bits))\n (No client certificate requested)\n by phobos.denx.de (Postfix) with ESMTPS id 48AE281426\n for <u-boot@lists.denx.de>; Tue,  4 Mar 2025 14:09:55 +0100 (CET)",
                    "by mail-io1-xd2e.google.com with SMTP id\n ca18e2360f4ac-85ae33109f6so128326139f.2\n for <u-boot@lists.denx.de>; Tue, 04 Mar 2025 05:09:55 -0800 (PST)",
                    "from chromium.org (c-73-203-119-151.hsd1.co.comcast.net.\n [73.203.119.151]) by smtp.gmail.com with ESMTPSA id\n ca18e2360f4ac-858753cd304sm287383839f.33.2025.03.04.05.09.49\n (version=TLS1_3 cipher=TLS_AES_256_GCM_SHA384 bits=256/256);\n Tue, 04 Mar 2025 05:09:50 -0800 (PST)"
                ],
                "X-Spam-Checker-Version": "SpamAssassin 3.4.2 (2018-09-13) on phobos.denx.de",
                "X-Spam-Level": "",
                "X-Spam-Status": "No, score=-2.1 required=5.0 tests=BAYES_00,DKIMWL_WL_HIGH,\n DKIM_SIGNED,DKIM_VALID,DKIM_VALID_AU,DKIM_VALID_EF,\n RCVD_IN_DNSWL_BLOCKED,SPF_HELO_NONE,SPF_PASS autolearn=ham\n autolearn_force=no version=3.4.2",
                "DKIM-Signature": "v=1; a=rsa-sha256; c=relaxed/relaxed;\n d=chromium.org; s=google; t=1741093792; x=1741698592; darn=lists.denx.de;\n h=content-transfer-encoding:mime-version:message-id:date:subject:cc\n :to:from:from:to:cc:subject:date:message-id:reply-to;\n bh=B2zsLws430/BEZfatNjeaNnrcxmYUstVjp1pSXgNQjc=;\n b=dG8yqtoKpSy15RHagnPcppzR8KbFCRXa2OBwXfwGoyN6M15tOJsUu2tpCdBFYiL5Mk\n hQz5iDLV8p0Bs+fP4XtNEx7KeYfTZhiqcRFvdCLwYtGray/IHtOZaNoHLajrstic/OgE\n 01ymu6gOEboU32eQ8uC8pdCYQ4UCkfKJwmiiU=",
                "X-Google-DKIM-Signature": "v=1; a=rsa-sha256; c=relaxed/relaxed;\n d=1e100.net; s=20230601; t=1741093792; x=1741698592;\n h=content-transfer-encoding:mime-version:message-id:date:subject:cc\n :to:from:x-gm-message-state:from:to:cc:subject:date:message-id\n :reply-to;\n bh=B2zsLws430/BEZfatNjeaNnrcxmYUstVjp1pSXgNQjc=;\n b=eihzJf4i9gin9usvz4hnAvvbLV9/yB7hGPpwwW/amgnPUyWCeQstgvGL7WDLYYnukH\n 161p4mt7+cCj7Hao/jSPvVZeuKiBNPkS4YCuP3QjXfdk2ziQ9IjloVmGarWZUOlYJ5iQ\n dZnxypUkuFfLcEDSwUmRO1dvLi3nH8PDlae3yT2H87LeHaxhXWdzHxQdPc86rkYyCqCr\n qBC2CTS31jqSuiaI+7qB3glvbJbSEXkunz0iDewTJDvZfmuloxTipWUjRJ1mg9UJcZt5\n 9xIuTq1n9aYf1RcQlrEOQhdBAQ0/IJgvmZtzPZi9L+ppBva1ER/xm06nMA7GEUtyGwun\n c6pA==",
                "X-Gm-Message-State": "AOJu0Yybx3b1+yClf/IfIbQd9u8sxzK9ixPP2HimXF/dGZfSiS7Cb+O5\n WrAkvtp7m3KPM/Mpv0sSZ5qrfTnKnb3WZyv6Oe5Q1iUjAftGNwbSxob5eJ/0y3cgrTdzE4sIWPE\n =",
                "X-Gm-Gg": "ASbGncu5gtgpXEPGrpbTRJulqFrFj1YPAAmKk4MiXA8/3J1A+25F0Uug2KeFUrZEjkG\n KMdPg/C7e2emIvfM+Jl+mKv0ITBvhbyNCyY1q2U1s1cayZF05coZ9ewzGxXJGiEqLMG69uBmmIi\n rBEvCnkXS+HVZobDQMtOsezpc+Ju8JRA7+y1R0WIlutl1mQARct6p0zTkuZp75QyB6dm/d0KYgd\n iux/t/f0HC2CxstQlTlJYzKL6UJgkB5/UorY1lW/0NDRS6P1iemPQ7I3EPLJO8tM5ZrpJE7qgNP\n xy0jXbUv44c48qJ1VszfY5USB8fRG7nwUYxNu6N1PXv9xWbl+z2xL68qNYUrFlHsB8ILTXAyzyr\n Cdj+Sxg==",
                "X-Google-Smtp-Source": "\n AGHT+IFeVk5D4YEfJgPxOfg3ikO6Q7IhaDzABGkAPI6HA0ubK85OPhUHK08gV7enBQ8OdoE/ttqEjw==",
                "X-Received": "by 2002:a05:6602:640f:b0:855:63c8:abb5 with SMTP id\n ca18e2360f4ac-85881fdba3amr1839428939f.13.1741093792636;\n Tue, 04 Mar 2025 05:09:52 -0800 (PST)",
                "From": "Simon Glass <sjg@chromium.org>",
                "To": "U-Boot Mailing List <u-boot@lists.denx.de>",
                "Cc": "Simon Glass <sjg@chromium.org>, Alexander Kochetkov <al.kochet@gmail.com>,\n Alper Nebi Yasak <alpernebiyasak@gmail.com>,\n Brandon Maier <brandon.maier@collins.com>,\n Jerome Forissier <jerome.forissier@linaro.org>,\n Jiaxun Yang <jiaxun.yang@flygoat.com>,\n Neha Malcom Francis <n-francis@ti.com>,\n Patrick Rudolph <patrick.rudolph@9elements.com>,\n Paul HENRYS <paul.henrys_ext@softathome.com>, Peng Fan <peng.fan@nxp.com>,\n Philippe Reynes <philippe.reynes@softathome.com>,\n Stefan Herbrechtsmeier <stefan.herbrechtsmeier@weidmueller.com>,\n Tom Rini <trini@konsulko.com>",
                "Subject": "[PATCH 0/7] binman: Check code-coverage requirements",
                "Date": "Tue,  4 Mar 2025 06:09:37 -0700",
                "Message-ID": "<20250304130947.109799-1-sjg@chromium.org>",
                "X-Mailer": "git-send-email 2.43.0",
                "MIME-Version": "1.0",
                "Content-Transfer-Encoding": "8bit",
                "X-BeenThere": "u-boot@lists.denx.de",
                "X-Mailman-Version": "2.1.39",
                "Precedence": "list",
                "List-Id": "U-Boot discussion <u-boot.lists.denx.de>",
                "List-Unsubscribe": "<https://lists.denx.de/options/u-boot>,\n <mailto:u-boot-request@lists.denx.de?subject=unsubscribe>",
                "List-Archive": "<https://lists.denx.de/pipermail/u-boot/>",
                "List-Post": "<mailto:u-boot@lists.denx.de>",
                "List-Help": "<mailto:u-boot-request@lists.denx.de?subject=help>",
                "List-Subscribe": "<https://lists.denx.de/listinfo/u-boot>,\n <mailto:u-boot-request@lists.denx.de?subject=subscribe>",
                "Errors-To": "u-boot-bounces@lists.denx.de",
                "Sender": "\"U-Boot\" <u-boot-bounces@lists.denx.de>",
                "X-Virus-Scanned": "clamav-milter 0.103.8 at phobos.denx.de",
                "X-Virus-Status": "Clean"
            content (str): Email content, e.g. 'This series adds a cover-coverage check to CI for Binman. The iMX8 tests\nare still not completed,...'
        """
        return self.request(f'covers/{cover_id}/')

    def get_cover_comments(self, cover_id):
        """Read comments about a cover letter

        Args:
            cover_id (str): Patchwork cover-letter ID

        Returns: list of dict: list of comments, each:
            id (int): series ID unique across patchwork instance, e.g. 3472068
            web_url (str): Full URL, e.g. 'https://patchwork.ozlabs.org/comment/3472068/'
            list_archive_url: (unknown?)

            project (dict): project information (id, url, name, link_name,
                list_id, list_email, etc.
            url (str): Full URL, e.g. 'https://patchwork.ozlabs.org/api/1.2/covers/2054866/'
            web_url (str): Full URL, e.g. 'https://patchwork.ozlabs.org/project/uboot/cover/20250304130947.109799-1-sjg@chromium.org/'
            project (dict): project information (id, url, name, link_name,
                list_id, list_email, etc.
            date (str): Date, e.g. '2025-03-04T13:16:15'
            subject (str): 'Re: [PATCH 0/7] binman: Check code-coverage requirements'
            submitter (dict): id, url, name, email, e.g.:
                "id": 6170,
                "url": "https://patchwork.ozlabs.org/api/people/6170/",
                "name": "Simon Glass",
                "email": "sjg@chromium.org"
            content (str): Email content, e.g. 'Hi,\n\nOn Tue, 4 Mar 2025 at 06:09, Simon Glass <sjg@chromium.org> wrote:\n>\n> This '...
            headers: dict: email headers, see get_cover() for an example
        """
        return self.request(f'covers/{cover_id}/comments/')

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
        data = self.get_patch(patch_id)
        state = data['state']
        data = self.request(f'patches/{patch_id}/comments/')
        num_comments = len(data)

        result[seq] = PATCH(patch_id, state, num_comments)
        done = len([1 for i in range(len(result)) if result[i]])

        if self._show_progress:
            terminal.tprint(f'\r{count - done}  ', newline=False)

    def series_get_state(self, series_id):
        """Sync the series information against patchwork, to find patch status

        Args:
            series_id (str): Patchwork series ID

        Return: tuple:
            COVER object, or None
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

        cover = data['cover_letter']
        cover_id = None
        if cover:
            cover_id = cover['id']
            info = self.get_cover_comments(cover_id)
            cover = COVER(cover_id, len(info), cover['name'])

        return cover, result
