# Build environment for pico-usb2swire (Pico SDK + ARM toolchain + picotool).
#
#   docker build -t pico-usb2swire-build .
#   docker run --rm -u $(id -u):$(id -g) -v "$PWD":/src pico-usb2swire-build
#
# Output: build/pico-usb2swire.uf2

FROM debian:trixie-slim

ARG PICO_SDK_VERSION=2.3.0
ARG PICOTOOL_VERSION=2.3.0

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git ca-certificates python3 \
        gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
        libusb-1.0-0-dev pkg-config \
    && rm -rf /var/lib/apt/lists/* \
    # Keep only the Cortex-M0+ (thumb/v6-m) multilibs; the others are ~2.5 GB
    && for d in /usr/lib/arm-none-eabi/newlib /usr/lib/gcc/arm-none-eabi/*; do \
         find $d/thumb -mindepth 1 -maxdepth 1 ! -name v6-m -exec rm -rf {} + ; \
         rm -rf $d/arm ; \
       done

ENV PICO_SDK_PATH=/opt/pico-sdk
RUN git clone --depth 1 --branch ${PICO_SDK_VERSION} https://github.com/raspberrypi/pico-sdk ${PICO_SDK_PATH} \
    && git -C ${PICO_SDK_PATH} submodule update --init --depth 1 lib/tinyusb

# Install picotool so the SDK uses it instead of fetching and building it on every build
RUN git clone --depth 1 --branch ${PICOTOOL_VERSION} https://github.com/raspberrypi/picotool /tmp/picotool \
    && cmake -S /tmp/picotool -B /tmp/picotool/build -G Ninja \
        -DCMAKE_INSTALL_PREFIX=/opt/picotool -DPICOTOOL_FLAT_INSTALL=1 \
    && cmake --build /tmp/picotool/build \
    && cmake --install /tmp/picotool/build \
    && rm -rf /tmp/picotool
ENV picotool_DIR=/opt/picotool/picotool
ENV PATH=/opt/picotool/picotool:${PATH}

WORKDIR /src
CMD ["sh", "-c", "cmake -S . -B build -G Ninja && cmake --build build"]
