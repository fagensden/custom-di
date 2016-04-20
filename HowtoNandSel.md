**Howto use the nand selection feature:**

Create a new folder in the root of the device holding your emunand called "nands"

For ONEEKs: sd:/nands
For ONEEKu: usb:/nands

Create a subfolder for every emunand you decided to use in the folder "nands" and copy all nand folders with it contents into it.

The folder structure should for example now look like this:

  * nands
  * ./nand1
  * .../import
  * .../meta
  * .../shared1
  * .../shared2
  * .../sys
  * .../ticket
  * .../title
  * .../tmp
  * ./nand2
  * .../import
  * .../meta
  * .../shared1
  * .../shared2
  * .../sys
  * .../ticket
  * .../title
  * .../tmp

When using a clean nand (for example created with ModMii by XFlak) it is possible the folders import, meta, sys and tmp are not available.
In this case just use the folders that are available.

Now launch the modules as normal. If there is an emunand available in the root of your device this one will be ignored. De kernel will create a new config file in the /sneek folder called "nandcfg.bin". This file contains all the information the modules need to work with the nand selection feature. Ones the modules are done loading they should have loaded one of the available emunands in the nands folder.

When loading a clean nand finish the initial setup.

To select a nand to work with. Just enter the overlay menu. On the ONEEK options page there should be an option that shows the emunand currently in use. Go to that option and you should be able to choose between the installed emunands using left and right of the d-pad on your wiimote. Now just press A to activate the emunand selected. This will restart ONEEK and load it with the selected nand.


ModMii by XFlak has some great features to create some emunand that work with these module. You can create as many nands and from any region you wish.

For more info about ModMii please visit:

http://gbatemp.net/topic/207126-modmii-for-windows/