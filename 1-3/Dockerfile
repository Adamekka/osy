FROM alpine:latest

RUN apk add --no-cache build-base

WORKDIR /app

# Copy only build inputs so host binaries cannot be reused in the Linux build.
COPY Makefile main.cpp log.hpp log.cpp run ./

RUN make

# Root can read mode-000 files, so test permissions as an unprivileged user.
USER nobody

CMD ["sh", "./run"]
