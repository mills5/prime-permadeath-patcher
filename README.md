# prime-permadeath-patcher #

Patches Metroid Prime 1 for instant death on any damage taken, with the option to disable saves.

![](https://i.imgur.com/EZK3zRZ.gif)

## Requirements ##

A valid Metroid Prime GameCube .ISO file, of one of the below versions supported by the patcher:

- USA v1.0 (Disc 0, Revision 0)

- USA v1.1 (Disc 0, Revision 1)

The European, Japanese, and USA v1.2 releases are not currently supported.

This patcher will work with .isos that already have other mods, randomizers, patches, etc installed.

## How To Use ##

### GUI (Windows only) ###

1. Run `prime-permadeath-patcher.exe`.
2. Select the .iso file you wish to patch or unpatch. The program will automatically detect if the patches are already installed.
3. Follow the on-screen instructions.

### Command Line ###

`prime-permadeath-patcher <.iso file> <install/remove permadeath... 1|0> <install/remove no-saving... 1|0>`

Example Usage:

Install Permadeath Patch Only, Don't Disable Saves

`prime-permadeath-patcher metroid_prime.iso 1 0`

Install Permadeath Patch AND Disable Saves

`prime-permadeath-patcher metroid_prime.iso 1 1`

Uninstall All Patches / Revert To Vanilla

`prime-permadeath-patcher metroid_prime.iso 0 0`