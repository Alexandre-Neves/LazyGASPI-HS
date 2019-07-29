files=( "write" "read" "init" "general" "prefetch")
libname="libLazyGASPI_HS.a"
flags="-Wall -Wno-unused-function -Wextra -Wfatal-errors"

#Clear bin folder
rm -f bin/*.o bin/*.a

#Compile files
for file in "${files[@]}" 
do
    echo "Compiling ${file}.cpp..."
	g++ -pthread $flags $@ -Iinclude -o "bin/${file}.o" -c "src/${file}.cpp" -Llib64 -lGPI
done

#Make library
ar rcs bin/$libname bin/*.o

#Move to lib folder (erase previous)
rm -f lib/$libname
mv bin/$libname lib64/$libname

