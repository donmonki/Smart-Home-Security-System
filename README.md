# Smart-Home-Security-System
Project work repository for developing a Smart Home Security System - 34346 Networking technologies and application development for IoT 


# Github Development guide

!IMPORTANT!
Please do your developments on **branches** and when you are done with that, create a pull request for merging back to the main!


For each component you are developing, create a different project folder using PlatformIO in the repository.

In each PlatformIO project folder which requires to use the shared communication library, in the `platformio.ini` file include the following line:

`lib_extra_dirs = ../Adapter_Lib`

For the usage of the shared communication library, please refer to the library documentation and example files within the `Adapter_Lib` folder.