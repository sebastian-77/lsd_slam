/**
*
* Based on original LSD-SLAM code from:
* Copyright 2013 Jakob Engel <engelj at in dot tum dot de> (Technical University of Munich)
* For more information see <http://vision.in.tum.de/lsdslam>
*
* LSD-SLAM is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* LSD-SLAM is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with LSD-SLAM. If not, see <http://www.gnu.org/licenses/>.
*/

#include <boost/thread.hpp>

#include "App/g3logger.h"

#include "SlamSystem.h"

#include "util/settings.h"
#include "util/Parse.h"
#include "util/globalFuncs.h"
#include "util/ThreadMutexObject.h"
#include "util/Configuration.h"
#include "util/FileUtils.h"

#include "LSD_GUI/GuiThread.h"
#include "LSD_GUI/Pangolin_IOWrapper/PangolinOutput3DWrapper.h"
#include "LSD_GUI/Pangolin_IOWrapper/PangolinOutputIOWrapper.h"


#include "App/InputThread.h"
#include "ParseArgs.h"

ThreadSynchronizer startAll;

using namespace lsd_slam;

int main( int argc, char** argv )
{
  G3Logger logWorker( argv[0] );
  logWorker.logBanner();

  Configuration conf;
  ParseArgs args( argc, argv );

  // Load configuration for LSD-SLAM
  conf.inputImage = args.undistorter->inputImageSize();
  conf.slamImage  = args.undistorter->outputImageSize();
  conf.camera     = args.undistorter->getCamera();

  LOG(INFO) << "Slam image: " << conf.slamImage.width << " x " << conf.slamImage.height;

  CHECK( (conf.camera.fx) > 0 && (conf.camera.fy > 0) ) << "Camera focal length is zero";

  std::shared_ptr<SlamSystem> system( new SlamSystem(conf) );

  // GUI elements need to ebe initialized in main thread on OSX
  std::shared_ptr<GUI> gui( new GUI( system->conf() ) );

  std::shared_ptr<lsd_slam::PangolinOutput3DWrapper> outputWrapper( new PangolinOutput3DWrapper( system->conf(), *gui ) );
  system->set3DOutputWrapper( outputWrapper.get() );

  std::shared_ptr<lsd_slam::PangolinOutputIOWrapper> ioWrapper( new PangolinOutputIOWrapper( system->conf(), *gui ) );

  LOG(INFO) << "Starting input thread.";
  InputThread input( system, args.dataSource, args.undistorter );
  input.setIOOutputWrapper( ioWrapper );
  boost::thread inputThread( boost::ref(input) );
  input.inputReady.wait();

  // Wait for all threads to be ready.
  LOG(INFO) << "Starting all threads.";
  startAll.notify();

  while( !pangolin::ShouldQuit() && !input.inputDone.getValue() ) {
    gui->update();
  }

  LOG(INFO) << "Finalizing system.";
  system->finalize();


  return 0;
}
