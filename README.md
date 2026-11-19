<div align=center>

![banner](banner.png)  
Way too much in one place obviously, this is pasted on top of pasted  
Use it how you wish, This will continue to be maintained just open source now  

Build it with Clang-cl (Get it from Visual Studio LLVM Packages)  
A build script has been provided for your use  
It will make both AVX2 and SSE DLLs inside a "Dist" folder  

Crashed? Open an Issue  

Discord? No, not after what happened to the first one  

AstralMod follows a Bleeding Edge workflow, Expect bugs and issues  
Feel free to report them in the Issues  

# Loader
Our loader is built using regular `MSBUILD`, It sends information to the cheat for your `Username`  
The loader can also load any DLLs in the same folder as itself, So have fun with it  
Our loader has support for themida out of the box, It is based on the Framework Unified Loader  
Release x64 will compile without Themida for the very reason that we cannot safely bundle the ThemidaSDK  
If you have your own copy of Themida you can use Secure x64 after placing the SDK in the following place  
`FUL Astral/FUL/vendor/ThemidaSDK` Themida will know what to do when you load the binary into it  
**YES WE KNOW IT TAKES TIME TO INJECT, ITS BECAUSE WE HIJACK THREADS FOR MANUALMAPPING**  

# Website
The website will never be open source, It's not important enough to care about  

# Playerlist/Database
We use Lunaris' public database and our own database for player marks  