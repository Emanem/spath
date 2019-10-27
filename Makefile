#Makefile generated by amake
#On Sun Oct 27 18:14:17 2019
#To print amake help use 'amake --help'.
CC=gcc
CPPC=g++
LINK=g++
SRCDIR=src
OBJDIR=obj
FLAGS=-g -Wall -std=c++11 -pthread -Wno-narrowing 
LIBS=-lglut -lGL -lOpenCL 
OBJS=$(OBJDIR)/main.o $(OBJDIR)/cl_renderer.o $(OBJDIR)/cpu_renderer.o 
EXEC=spath
DATE=$(shell date +"%Y-%m-%d")

$(EXEC) : $(OBJS)
	$(LINK) $(OBJS) -o $(EXEC) $(FLAGS) $(LIBS)

$(OBJDIR)/main.o: src/main.cpp src/cpu_renderer.h src/renderer.h src/scene.h \
 src/geom.h src/view.h src/cl_renderer.h $(OBJDIR)/__setup_obj_dir
	$(CPPC) $(FLAGS) src/main.cpp -c -o $@

$(OBJDIR)/cl_renderer.o: src/cl_renderer.cpp src/basic_renderer.h src/renderer.h \
 src/scene.h src/geom.h src/view.h src/cl_renderer.h $(OBJDIR)/__setup_obj_dir
	$(CPPC) $(FLAGS) src/cl_renderer.cpp -c -o $@

$(OBJDIR)/cpu_renderer.o: src/cpu_renderer.cpp src/cpu_renderer.h src/renderer.h \
 src/scene.h src/geom.h src/view.h src/basic_renderer.h src/frand.h $(OBJDIR)/__setup_obj_dir
	$(CPPC) $(FLAGS) src/cpu_renderer.cpp -c -o $@

$(OBJDIR)/__setup_obj_dir :
	mkdir -p $(OBJDIR)
	touch $(OBJDIR)/__setup_obj_dir

.PHONY: clean bzip release

clean :
	rm -rf $(OBJDIR)/*.o
	rm -rf $(EXEC)

bzip :
	tar -cvf "$(DATE).$(EXEC).tar" $(SRCDIR)/* Makefile
	bzip2 "$(DATE).$(EXEC).tar"

release : FLAGS +=-O3 -D_RELEASE
release : $(EXEC)

