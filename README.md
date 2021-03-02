# WriteToWoz

A command-line tool to directly write binary to a WOZ image heavely based on DSK2WOZ (c) 2018 Thomas Harte.

v0.30
[Custom 32 sectors/128 bytes with custom GAPS]

**Code:** GROUiK (FRENCH TOUCH) / Thomas Harte


MIT License
<br/>
<br/>
## Usage:

W2W.exe s d track sector image.woz binary.b [-v]

- [s]: standard track(s) / [c]: custom track(s)
- interleaving: [d] dos / [p]: physical / [i1]: custom1
- first [track] number
- first [sector] number
- image.woz name
- binary.b name
- -v verbose mode (optional)

warning: the first six parameters are mandatory! 
<br/>
<br/>
## Building Instructions:

Open solution under Visual Studio Community (Windows 10).  
Can probably be ported to other platforms very easily.
<br/>
<br/>
## Disclaimer:

This tool has been written to help Apple II cross-development on PC and is made to be executed from a command line.   
It is not optimized and probably buggy under certain circumstances.  
Use it at your own risk!