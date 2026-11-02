# Third-party notices and dependencies

- C++ instrument first-party MIT license: LICENSE and src/linux/apps/instrument_cpp/LICENSE.
- Linux out-of-tree TX/RX modules: GPL-2.0-only per SPDX declarations. The GPL terms and dependency notices supplied with the archived backend are retained in src/linux/tx_20261030/third_party.
- DejaVu font: src/linux/apps/instrument_cpp/resources/LICENSE_DEJAVU (Bitstream Vera/DejaVu terms). The bundled font is a runtime resource, not an executable.
- Board clock register tables/vendor material: corresponding clock/vendor/LICENSE, also preserved in Qt resources.
- AMD RFDC library: obtain the PetaLinux2025.2 librfdc recipe and matching headers/library; original license and recipe provenance retained under third_party/librfdc.
- libmetal-xlnx: BSD-3-Clause, matching PetaLinux recipe; notices retained under third_party/libmetal-xlnx.
- sysfsutils: associated GPL/LGPL texts and source archive retained under third_party. Rebuild the recipe matching the captured PetaLinux image.
- Qt5: installed from the matching PetaLinux SDK; this source-only bundle does not redistribute the Qt runtime. Follow its LGPL/GPL/commercial terms as applicable for separate runtime distribution.
- AMD Vivado, IP catalog and PetaLinux toolchain are external dependencies under their original terms. Generated vendor PS initialization/SDT files preserve their own notices. Proprietary IP implementation, encrypted cores and tool installers are not copied here.
- The original DDR board constraints are imported from the ZCU111 evaluation reference already used in the frozen project, not newly authored as part of V16. Preserve provenance when publishing the repository.

No blanket MIT claim overrides individual vendor headers, font, kernel or dependency licenses. This file is a dependency map; each retained license remains authoritative.
