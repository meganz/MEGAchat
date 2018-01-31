TEMPLATE = subdirs

SUBDIRS += MEGAchatTests

system("cmake -P ../../src/genDbSchema.cmake")
