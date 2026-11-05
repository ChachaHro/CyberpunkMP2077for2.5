FROM mcr.microsoft.com/dotnet/sdk:9.0-bookworm-slim-amd64 AS build
WORKDIR /app
ENV XMAKE_ROOT=y
ENV DOTNET_ROOT=/usr/share/dotnet
ENV PATH=$DOTNET_ROOT:$PATH

RUN echo 'deb http://deb.debian.org/debian bookworm-backports main' >> /etc/apt/sources.list
RUN apt update \
  && apt install -y xmake g++ unzip wget ca-certificates git \
  && wget -q https://dot.net/v1/dotnet-install.sh -O /tmp/dotnet-install.sh \
  && bash /tmp/dotnet-install.sh --channel 8.0 --runtime dotnet --install-dir /usr/share/dotnet \
  && rm /tmp/dotnet-install.sh \
  && apt clean

COPY . .

RUN --mount=type=cache,target=/root/.xmake xmake -y

# RELEASE STAGE
# 8.0 (pas une version figée) : l'image suit les correctifs de sécurité du runtime
FROM mcr.microsoft.com/dotnet/runtime:8.0-bookworm-slim-amd64 AS release

# Utilisateur sans privilèges. /app lui appartient : le serveur y écrit sa config, ses journaux et les
# mods téléchargés.
RUN groupadd --system --gid 10001 cpmp \
 && useradd --system --uid 10001 --gid cpmp --home-dir /app --shell /usr/sbin/nologin cpmp

WORKDIR /app
COPY --from=build --chown=cpmp:cpmp /app/build/linux/x86_64/release/ /app/

USER cpmp:cpmp
ENV DOTNET_CLI_TELEMETRY_OPTOUT=1

# Jeu en UDP, API web en TCP, sur le même numéro de port
EXPOSE 11778/udp 11778/tcp
HEALTHCHECK --interval=30s --timeout=5s --start-period=60s --retries=3 \
  CMD bash -c 'exec 3<>/dev/tcp/127.0.0.1/11778' || exit 1

CMD [ "/app/Server.Loader" ]
