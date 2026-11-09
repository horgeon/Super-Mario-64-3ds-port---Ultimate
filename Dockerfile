FROM devkitpro/devkitarm:latest

RUN apt-get update && \
    apt-get install -y \
      binutils-mips-linux-gnu \
      bsdmainutils \
      build-essential \
      unzip \
      zlib1g-dev

RUN wget https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.17/makerom-v0.17-ubuntu_x86_64.zip \
  -O makerom.zip && \
  echo 976c17a78617e157083a8e342836d35c47a45940f9d0209ee8fd210a81ba7bc0  makerom.zip | sha256sum --check && \
  unzip -d /opt/devkitpro/tools/bin/ makerom.zip && \
  chmod +x /opt/devkitpro/tools/bin/makerom && \
  rm makerom.zip

RUN wget https://github.com/diasurgical/bannertool/releases/download/1.2.0/bannertool.zip \
  -O bannertool.zip && \
  echo 69768596f836acb3e3aeaa66e47c6ba560dde813c6dfcd33c8afc25fe29b7524  bannertool.zip | sha256sum --check && \
  unzip -d /tmp/bannertool bannertool.zip && \
  mv /tmp/bannertool/linux-x86_64/bannertool /opt/devkitpro/tools/bin/bannertool && \
  chmod +x /opt/devkitpro/tools/bin/bannertool && \
  rm -rf /tmp/bannertool bannertool.zip
