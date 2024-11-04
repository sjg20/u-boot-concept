GitLab CI / U-Boot runner container
===================================

In order to have a reproducible and portable build environment for CI we use a container for building in.  This means that developers can also reproduce the CI environment, to a large degree at least, locally.  This file is located in the tools/docker directory.

The docker image supports both amd64 and arm64. Ensure that the
'docker-buildx' Debian package is installed (or the equivalent on another
distribution).

Building is currently only supports on an amd64 machine (i.e. 64-bit x86). While
both amd64 and arm64 happen in parallel, the arm64 part will take considerably
longer as it must use QEMU to emulate the arm64 code.

To build the image yourself::

.. code-block:: bash

    docker buildx build --platform linux/arm64/v8,linux/amd64 -t your-namespace:your-tag .

Or to use an existing container

.. code-block:: bash

    docker pull trini/u-boot-gitlab-ci-runner:jammy-20240227-14Mar2024
