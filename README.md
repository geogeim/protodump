# protodump

extracts .proto files from binaries  
usage: protodump \<file file...\> \[options\]  
options:  
-v                      verbose  
-o \<dir\>                output directory, will be created if missing, defaults to current  
--descriptor-proto      also dump descriptor.proto  
--unknown-dependencies  dump definitions even if there are missing dependencies  
                        they will be replaced with dummy descriptors  
