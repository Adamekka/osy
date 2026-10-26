# The supplied library requires Linux x86-64, including on Apple Silicon Macs.
FROM --platform=linux/amd64 gcc:14

WORKDIR /app

# Exclude host object files so the build always uses Linux objects.
COPY Makefile main.cpp io.hpp zoo_client.cpp zoo_info.cpp zoo_map.hpp libzoomap.so ./

RUN make

CMD ["bash"]
