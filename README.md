# CSNV's webtoken plugin

Webtoken plugin for the Hercules emulator.

This plugin is required for third party applications such as [Mindu](https://github.com/csnv/Mindu/) or [athena-web-service](https://github.com/secretdataz/athena-web-service) to provide the web services required by 2019-06+ Ragexe clients for emblems, character and user configurations in the Hercules emulator.

> **Note**
>
> This project only aims to provide a bare minimun solution for 2019-06+ clients.
For a more functional, feature rich, better security and better performance solution, help funding the real deal here https://board.herc.ws/topic/20151-http-support-in-hercules-funding/

## How to
1. Run the file login_webtoken_fields.sql against your server database. Skip step 2 if your Hercules version is equal or greater than 202301110.
2. Apply webtoken.diff into your Hercules codebase. Alternatively, you can directly apply this [PR](https://github.com/HerculesWS/Hercules/pull/3183).
3. Place csnv_webtoken.c in your src/plugins directory and compile, per the Hercules' docs:
  - [Old Wiki] https://wiki.herc.ws/wiki/Hercules_Plugin_Manager
  - [Github Wiki, GCC] https://github.com/HerculesWS/Hercules/wiki/Building-HPM-Plugin-for-gcc
  - [Github Wiki, msvc] https://github.com/HerculesWS/Hercules/wiki/Building-HPM-Plugin-for-MSVC