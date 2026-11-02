FROM gcc:14

WORKDIR /app

# Copy source files explicitly so host object files cannot replace Linux builds.
COPY Makefile main.cpp client.cpp io.hpp ./
RUN make

EXPOSE 12345
CMD ["./pizzeria", "12345"]
