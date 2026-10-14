FROM wolf3d-portable-build-windows-mingw-debian12
RUN apt-get update && apt-get install -y --no-install-recommends curl ca-certificates && rm -rf /var/lib/apt/lists/*
RUN curl -fL https://www.libsdl.org/release/SDL-1.2.15.tar.gz -o /tmp/SDL.tar.gz && \
 echo 'd6d316a793e5e348155f0dd93b979798933fb98aa1edebcc108829d6474aad00  /tmp/SDL.tar.gz' | sha256sum -c - && \
 tar -xf /tmp/SDL.tar.gz -C /tmp && cd /tmp/SDL-1.2.15 && \
 CFLAGS='-O2 -D_WIN32_WINNT=0x0500 -DWINVER=0x0500' LDFLAGS='-static-libgcc' \
 ./configure --host=i686-w64-mingw32 --prefix=/opt/sdl1 --disable-static --enable-shared && \
 make -j4 && make install && cp COPYING /opt/sdl1/COPYING && \
 grep '#define SDL_VIDEO_DRIVER_DDRAW 1' include/SDL_config.h && \
 grep '#define SDL_VIDEO_DRIVER_WINDIB 1' include/SDL_config.h
# Configure libtool's stored compiler command as well as the Makefile command.
RUN cd /tmp/SDL-1.2.15 && make clean && \
 sed -i 's/ -shared / -shared -static-libgcc /g; s/-lgcc_s/-lgcc/g' libtool && \
 make -j4 && make install && \
 ( ! i686-w64-mingw32-objdump -p /opt/sdl1/bin/SDL.dll | grep 'DLL Name: libgcc' )
