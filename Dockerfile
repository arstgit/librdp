FROM gcc
RUN apt-get update && apt-get install sudo -y
COPY . /app
WORKDIR /app
RUN make BUILD=debug && make install
