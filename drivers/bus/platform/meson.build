# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 Realtek Semiconductor Corp.

deps += ['platform']
headers = files('rte_bus_platform.h')
sources = files(
        'platform_common.c',
        'platform_common_uio.c',
)
if is_linux
    sources += files(
            'linux/platform.c',
            'linux/platform_uio.c',
    )
    includes += include_directories('linux')
endif

deps += ['kvargs']
