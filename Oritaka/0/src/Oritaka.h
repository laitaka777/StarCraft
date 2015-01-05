#pragma once

#include <windows.h>
#include <BWAPI.h>
#include <BWTA.h>
//#include <BWSAL.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>

#define MAX_ID             65536
#define MAX_BUILDING       65536
#define MAX_MAP_WIDTH      256
#define MAX_MAP_HEIGHT     256
#define MAX_BASE           256
#define MAX_RESOURCE_DEPOT 16


class Oritaka : public BWAPI::AIModule
{
public:
  virtual void onStart();
  virtual void onFrame();
  virtual void onSendText(std::string text);
private: 
};
