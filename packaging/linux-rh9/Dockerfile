FROM debian:10-slim@sha256:bb3dc79fddbca7e8903248ab916bb775c96ec61014b3d02b4f06043b604726dc
RUN printf 'deb http://archive.debian.org/debian buster main\ndeb http://archive.debian.org/debian-security buster/updates main\n' > /etc/apt/sources.list \
 && apt-get -o Acquire::Check-Valid-Until=false update \
 && apt-get install -y --no-install-recommends gcc-multilib make binutils curl ca-certificates rpm2cpio cpio xz-utils python3 \
 && rm -rf /var/lib/apt/lists/*
COPY SHA256SUMS /tmp/inputs/SHA256SUMS
RUN cd /tmp/inputs && while read hash name; do \
 case "$name" in *.rpm) url="https://legacy.redhat.com/pub/redhat/linux/9/en/os/i386/RedHat/RPMS/$name" ;; \
 *) url="https://www.libsdl.org/release/$name" ;; esac; \
 curl -fLsS "$url" -o "$name" || exit 1; done < SHA256SUMS \
 && sha256sum -c SHA256SUMS \
 && mkdir -p /opt/rh9 && cd /opt/rh9 \
 && for rpm in /tmp/inputs/*.rpm; do rpm2cpio "$rpm" | cpio -idmu; done
# glibc linker scripts contain absolute target paths: make them sysroot-relative.
RUN ln -s . /opt/rh9/usr/lib/32 && sed -i 's@ /@ =/@g' /opt/rh9/usr/lib/libc.so /opt/rh9/usr/lib/libpthread.so \
 && curl -fLsS https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-linux-x86_64.tar.gz -o /tmp/cmake.tar.gz \
 && echo '5a1133ff103c71eb5120e2cc3de922733e7d8a26a98ae716397e8676adb367bf  /tmp/cmake.tar.gz' | sha256sum -c - \
 && mkdir /opt/cmake && tar -xf /tmp/cmake.tar.gz -C /opt/cmake --strip-components=1
ENV PATH="/opt/cmake/bin:${PATH}"
RUN cd /tmp/inputs && tar -xf SDL-1.2.15.tar.gz && cd SDL-1.2.15 \
 && CC='gcc -no-pie -m32 --sysroot=/opt/rh9 -B/opt/rh9/usr/lib/' \
 CFLAGS='-O2 -march=pentium -mno-mmx -mno-sse -mno-sse2 -mfpmath=387 -fno-stack-protector -fgnu89-inline' \
 LDFLAGS='-static-libgcc -Wl,--hash-style=sysv -L/opt/rh9/usr/X11R6/lib' \
 ./configure --host=i586-linux-gnu --prefix=/usr --disable-static \
 --disable-assembly --disable-input-events --disable-input-tslib --disable-video-opengl --disable-video-dga --disable-video-fbcon --disable-video-directfb \
 --disable-alsa --disable-pulseaudio --disable-esd --disable-arts --disable-nas \
 --disable-video-x11-xrandr --disable-video-x11-xinerama --disable-video-x11-xv \
 --x-includes=/opt/rh9/usr/X11R6/include --x-libraries=/opt/rh9/usr/X11R6/lib \
 && make -j4 && make DESTDIR=/opt/rh9 install \
 && cp COPYING /opt/rh9/usr/share/SDL12-COPYING.txt
RUN apt-get -o Acquire::Check-Valid-Until=false update \
 && apt-get install -y --no-install-recommends git \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
