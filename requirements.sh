sudo apt-get update

# cmake
sudo pip install --upgrade cmake

# ccmake
sudo apt-get install cmake-curses-gui

# autoconf
sudo apt-get install autoconf

# automake
sudo apt-get install automake

# libtool
sudo apt-get install libtool

# python
sudo apt-get install python2.7

# g++
sudo apt-get install g++

# gtest
sudo apt-get install libgtest-dev
cd /usr/src/gtest
sudo cmake CMakeLists.txt
sudo make
sudo cp lib/*.a /usr/lib
#sudo ln -s /usr/src/gtest/lib/libgtest.a /usr/lib/libgtest.a
#sudo ln -s /usr/src/gtest/lib/libgtest_main.a /usr/lib/libgtest_main.a

# other libs (Ubuntu 20.04.2)
sudo apt-get install libglib2.0-dev libgtk-3-dev libpulse-dev libasound2-dev libuv1-dev libcap-dev libfreeimage-dev ffmpeg
