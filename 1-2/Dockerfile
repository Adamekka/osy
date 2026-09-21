FROM alpine:latest

RUN apk add --no-cache build-base

WORKDIR /app

# Copy only build inputs so host binaries cannot be reused in the Linux build.
COPY Makefile main.cpp transform.hpp transform_upper.cpp transform_lower.cpp run ./

RUN make

CMD ["sh", "./run"]
