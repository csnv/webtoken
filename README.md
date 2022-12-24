# CSNV's webtoken plugin

Webtoken plugin for the Hercules emulator.

> **Note**
>
> This project only aims to provide a bare minimun solution for 2019-06+ clients.
For a more functional, feature rich, better security and better performance solution, help funding the real deal here https://board.herc.ws/topic/20151-http-support-in-hercules-funding/


## How to
- Run the file login_webtoken_fields.sql against your server database
- Place csnv_webtoken.c in your src/plugins directory and compile, per the Hercules' docs:
  - [Old Wiki] https://wiki.herc.ws/wiki/Hercules_Plugin_Manager
  - [Github Wiki, GCC] https://github.com/HerculesWS/Hercules/wiki/Building-HPM-Plugin-for-gcc
  - [Github Wiki, msvc] https://github.com/HerculesWS/Hercules/wiki/Building-HPM-Plugin-for-MSVC