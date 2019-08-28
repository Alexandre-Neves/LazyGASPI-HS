LIBNAME="LazyGASPI_HS"

allopts="hp:-:m:"

MAKE_INC=make.inc
MAKE_INC_TEMP=.$MAKE_INC.temp
MACHINEFILE=machinefile.bak
MACHINEFILE_TEMP=.$MACHINEFILE.temp

function print_usage {
cat << EOF
Usage:
       ./configure.sh [OPTIONS]

Where OPTIONS are:
        -h,--help               Prints this help message.

        -p                      Indicates the installation path for LazyGASPI. 
                                Default is /usr/local.

        -m x*node               Adds 'x' entries of 'node' to the machinefile. 
                                Only necessary if tests are made. Can be used 
                                multiple times.

        --with-mpi[=<path>]     Indicates that the library should be compiled 
                                with MPI support. If path is omitted, default 
                                location for mpicxx will be used.

        --gaspi-libname=<name>  The name of the GASPI library to use. This is 
                                useful if both plain-GASPI and GASPI with MPI
                                are installed.

        --libpath=<path>        The path to the GASPI library.

        --debug                 Sets the DEBUG macro during compilation. See 
                                documentation for more details.

        --debug-internal        Sets the DEBUG_INTERNAL macro during compilation. 
                                See documentation for more details.

        --debug-test            Sets the DEBUG_TEST macro during compilation. 
                                See documentation for more details.

        --debug-performance     Sets the DEBUG_PERF macro during compilaion. See
                                documentation for more details.

        --shared,--static       Indicates  if  the  library  will  be shared or 
                                static. Default is static.

        --eigen=<path>          Indicates a path to the Eigen headers (download 
                                from http://eigen.tuxfamily.org). Only necessary
                                if tests will be made.

        --with-lock             Library  is  compiled  with  LOCKED_OPERATIONS, 
                                which means that a lock will be set everytime a 
                                row is written to or read from. 

        --with-safety-checks    Library is compiled with SAFETY_CHECKS, which
                                means parameter values passed to LazyGASPI's
                                functions will be checked for their validity
                                (ID's out of bounds, nullptrs, etc...) and will
                                return if parameters are "invalid".
EOF
}

function print_mpi_not_found {
    echo "Could not find an installation of MPI."
    echo "Use --with-mpi=<path_to_mpi> to indicate the location of MPI."
}

function print_mpi_wrong_path {
    echo "Could not find a proper installation of MPI in $1"
}

if [ -f $MAKE_INC ]; then mv $MAKE_INC $MAKE_INC_TEMP; fi
if [ -f $MACHINEFILE ]; then mv $MACHINEFILE $MACHINEFILE_TEMP; fi

WITH_MPI=0
PREFIX="/usr/local"
LIB_STATIC=1
GASPI_LIBNAME="GPI2"
EIGEN="/usr/local/include/eigen3"
LIB_PATH="$PREFIX/lib"
INCL_PATH="$PREFIX/include"

while getopts $allopts opt; do
    case $opt in
    h)
        print_usage
        if [ -f $MAKE_INC_TEMP ]; then mv -f $MAKE_INC_TEMP $MAKE_INC; fi
        if [ -f $MACHINEFILE_TEMP ]; then mv -f $MACHINEFILE_TEMP $MACHINEFILE; fi
        exit 1
    ;;
    p)
        PREFIX=${OPTARG}
    ;;
    m)
        NODE_AMOUNT=${OPTARG%\**}
        NODE_NAME=${OPTARG#*\*}
        while [[ $NODE_AMOUNT > 0 ]]; do
            echo "$NODE_NAME" >> $MACHINEFILE
            NODE_AMOUNT=$(( $NODE_AMOUNT - 1 ))
        done
        echo "PREMADE_MF=$MACHINEFILE" >> $MAKE_INC
    ;;
    -)
        case ${OPTARG} in
        help)
            print_usage
            exit 1
        ;;
        with-mpi)
            which mpicxx > /dev/null 2>&1
            if [ $? != 0 ]; then
                print_mpi_not_found
                exit 1
            fi
            WITH_MPI=1
        ;;
        with-mpi=*)
            MPI_PATH=${OPTARG#*=}
            if [ ! -f $MPI_PATH/bin/mpicxx ]; then
                print_mpi_wrong_path $MPI_PATH
                exit 1
            fi
            WITH_MPI=1
        ;;
        gaspi-libname=*)
            GASPI_LIBNAME=${OPTARG#*=}
        ;;
        libpath=*)
            echo "LDFLAGS+=-L${OPTARG#*=}" >> $MAKE_INC
        ;;
        debug)
            echo "CXXFLAGS+=-DDEBUG" >> $MAKE_INC
        ;;
        debug-internal)
            echo "CXXFLAGS+=-DDEBUG_INTERNAL" >> $MAKE_INC
        ;;
        debug-test)
            echo "CXXFLAGS+=-DDEBUG_TEST" >> $MAKE_INC
        ;;
        debug-performance)
            echo "CXXFLAGS+=-DDEBUG_PERF" >> $MAKE_INC
        ;;
        static)
            LIB_STATIC=1
        ;;
        shared)
            LIB_STATIC=0
        ;;
        eigen=*)
            EIGEN=${OPTARG#*=}
        ;;
        with-lock)
            echo "CXXFLAGS+=-DLOCKED_OPERATIONS" >> $MAKE_INC
        ;;
        with-safety-checks)
            echo "CXXFLAGS+=-DSAFETY_CHECKS" >> $MAKE_INC
        ;;
        esac
    ;;
    \?)
        print_unknown_opt $opt
    ;;
    esac
done

echo "LIBNAME=$LIBNAME" >> $MAKE_INC
printf "$LIBNAME will be installed in $PREFIX as a "
if [ $LIB_STATIC = 1 ]; then
    LIBNAME+=".a"
    printf "static library (%s) " "$LIB_PATH/lib$LIBNAME" 
    printf "using -l$GASPI_LIBNAME...\n"
else
    LIBNAME+=".so"
    echo "CXXFLAGS+=-fPIC" >> $MAKE_INC
    printf "shared library (%s) " "$LIB_PATH/$LIBNAME" 
    printf "using -l$GASPI_LIBNAME...\n"
    echo "If the path cannot be found by the dynamic linker, put the library in one that can."
fi


if [ $WITH_MPI = 1 ]; then
    echo "CXXFLAGS+=-DWITH_MPI" >> $MAKE_INC
    if [ -z $MPI_PATH ]; then
        echo "CXX=mpicxx" >> $MAKE_INC
    else
        echo "CXX=$MPI_PATH/bin/mpicxx" >> $MAKE_INC
        echo "MPI_PATH=$MPI_PATH" >> $MAKE_INC
    fi
fi

echo "WITH_MPI=$WITH_MPI" >> $MAKE_INC
echo "INCLUDES+=-Iinclude" >> $MAKE_INC
echo "LDLIBS+=-l$GASPI_LIBNAME" >> $MAKE_INC
echo "PREFIX=$PREFIX" >> $MAKE_INC
echo "LIB_STATIC=$LIB_STATIC" >> $MAKE_INC
echo "EIGEN=$EIGEN" >> $MAKE_INC


rm -f $MAKE_INC_TEMP
rm -f $MACHINEFILE_TEMP
