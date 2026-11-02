FROM gcc:14

WORKDIR /app

# Copy source files explicitly so macOS build artifacts cannot enter the image.
COPY Makefile main.cpp ./
RUN make

CMD ["./a.out"]
