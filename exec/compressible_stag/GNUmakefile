# AMREX_HOME defines the directory in which we will find all the AMReX code.
# If you set AMREX_HOME as an environment variable, this line will be ignored
AMREX_HOME ?= ../../../amrex/

DEBUG         = FALSE
USE_MPI       = TRUE
USE_OMP       = FALSE
USE_CUDA      = FALSE
USE_HIP       = FALSE
COMP          = gnu
DIM           = 3
TINY_PROFILE  = FALSE
MAX_SPEC      = 8

USE_PARTICLES = FALSE
DO_TURB 	  = FALSE

include $(AMREX_HOME)/Tools/GNUMake/Make.defs

VPATH_LOCATIONS   += .
INCLUDE_LOCATIONS += .

include ../../src_compressible_stag/Make.package
VPATH_LOCATIONS   += ../../src_compressible_stag/
INCLUDE_LOCATIONS += ../../src_compressible_stag/

include ../../src_compressible/Make.package
VPATH_LOCATIONS   += ../../src_compressible/
INCLUDE_LOCATIONS += ../../src_compressible/

include ../../src_chemistry/Make.package
VPATH_LOCATIONS   += ../../src_chemistry/
INCLUDE_LOCATIONS += ../../src_chemistry/

include ../../src_MFsurfchem/Make.package
VPATH_LOCATIONS   += ../../src_MFsurfchem/
INCLUDE_LOCATIONS += ../../src_MFsurfchem/

include ../../src_rng/Make.package
VPATH_LOCATIONS   += ../../src_rng/
INCLUDE_LOCATIONS += ../../src_rng/


include ../../src_common/Make.package
VPATH_LOCATIONS   += ../../src_common/
INCLUDE_LOCATIONS += ../../src_common/

include $(AMREX_HOME)/Src/Base/Make.package

include ../../src_analysis/Make.package
VPATH_LOCATIONS   += ../../src_analysis/
INCLUDE_LOCATIONS += ../../src_analysis/

include $(AMREX_HOME)/Tools/GNUMake/Make.rules

ifeq ($(findstring cgpu, $(HOST)), cgpu)
  CXXFLAGS += $(FFTW)
endif

ifeq ($(USE_CUDA),TRUE)
  LIBRARIES += -lcufft
else ifeq ($(USE_HIP),TRUE)
  # Use rocFFT.  ROC_PATH is defined in amrex
  INCLUDE_LOCATIONS += $(ROC_PATH)/rocfft/include
  LIBRARY_LOCATIONS += $(ROC_PATH)/rocfft/lib
  LIBRARIES += -L$(ROC_PATH)/rocfft/lib -lrocfft
else
  LIBRARIES += -L$(FFTW_DIR) -lfftw3_mpi -lfftw3
endif

ifeq ($(DO_TURB), TRUE)
  DEFINES += -DTURB
endif

MAXSPECIES := $(strip $(MAX_SPEC))
DEFINES += -DMAX_SPECIES=$(MAXSPECIES)
