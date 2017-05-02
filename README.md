Reverse engineered code of L2OFF server Chronicles-1

This code is published for STUDY PURPOSE ONLY. If you want use this code, you should contact NCSoft.

**NOTE**: branch "legacy" contains only ORIGINALY decompiled code, with origin bugs, MSVC2008 required to build it.

**NOTE**: All fixes and improvements are going to "develop" branch. **MSVC2013** or higher is required to build it.

### Consists of 3 parts:
* generated/ - directory, which contains IDA (6.8) / Hex-Rays generated decompiled code, which is used as a reference to further creating of compilable code
* reversed/ - directory, which contains manualy fixed generated code - structure definitions, renamings, etc.
* src/ - directory, which contains manualy fixed compilable code

### How to Build
* install newest cmake
* go to the project's root directory
* run commands:
```
mkdir build
cd build
cmake ../
or to build with MSVC 2008:
cmake -G "Visual Studio 9 2008" ../

cmake --build .
cmake --build . --target install
```
* after that, you will have MSVC project inside build/ directory, and built ready-to-use L2AuthD inside build/install

# L2AuthD
* Reverse engineered code of L2AuthD.exe, build version: 40504
* All modules were originally decompiled, including Blowfish algorithm, MD5, original prints (including Korean), password encryptions (for DB access and user ones), packet handling, call-stack report during crash, mail server, INI-file reading, etc
* The ONLY foreign code is DES algorithm. I wasn't able to decompile it at 100%, so some other open-source implementation is used
* Orignal Blowfish key is used: 5F 3B 35 2E 5D 39 34 2D 33 31 3D 3D 2D 25 78 54 21 5E 5B 24. Note, that there are few packs with changed BF keys:
```
5F 3B 35 2E 5D 39 34 2D 33 31 3D 3D 2D 25 78 54 21 5E 5B 24
6E 63 73 6F 66 74 6C 69 6E 65 61 67 65 32 2E 63 6F 6D 20 20
5B 3B 27 2E 5D 39 34 2D 33 31 3D 3D 2D 25 26 40 21 5E 2B 5D
```

#L2LogD
To be done

#CacheD
To be done

#L2Server
To be done

#L2NPC
To be done

Contact me in order to receive other parts or to have ports for Interlude

You are welcome to contribute, just have a fun!
