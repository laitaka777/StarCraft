Date: 2014/08/09

Developer: Yoshitaka Hirai

Contact Address: laitaka777@gmail.com

Program Name: Oritaka

Version: 1.0

URL: https://code.google.com/p/oritaka/

Code Licence: GNU GPL v3

System Requirements: Windows 7 Enterprise 64bit

Software Requirements: 

- Visual C++ 2012 Express Edition
- StarCraft Brood War 1.16.1
- BWAPI 3.7.4
- BWTA 1.7.1
- Chaoslauncher 0.5.3 ( for StarCraft Brood War 1.16.1 ) 

Compile: 

1. Open oritaka.sln by Microsoft Visual Studio Express 2012 for Windows Desktop. 
2. Choose not to upgrade the projects (BWAPI 3.7.4 requires v90).
3. Set project properties as follows.

- Configuration: Release
- kind of configuration: .dll
- platform tool set: Visual Studio 2008(v90)
- C/C++ -> generation -> adding include directory: $(BWAPI_DIR)\include 
- Linker Input:  
  $(BWAPI_DIR)\lib\BWAPI.lib
  $(BWAPI_DIR)\lib\BWTA.lib
  $(BWAPI_DIR)\lib\tinyxmld.lib
  $(BWAPI_DIR)\lib\CGAL-vc90-mt.lib
  $(BWAPI_DIR)\lib\libboost_thread-vc90-mt-1_40.lib
  $(BWAPI_DIR)\lib\gmp-vc90-mt.lib
  $(BWAPI_DIR)\lib\mpfr-vc90-mt.lib

4. Set Environment Variable BWAPI_DIR.
   For example, BWAPI_DIR -> C:\Library\BWAPI\BWAPI 3.7.4
   To edit these in Windows 7,  right click "My Computer" -> "Properties" -> "Advanced System Settings" -> "Environment Variables".

5. Build "Oritaka".

Run:

1. Run Chaoslauncher.exe.
2. Modify Chaoslauncher configure to point to Oritaka.dll.
3. Run StarCraft by Chaoslauncher.


