# ====================================================================
# ESP32 开发环境 Docker 镜像
# 基于 ESP-IDF v5.4.1
# ====================================================================

FROM espressif/idf:release-v5.4

# 设置工作目录
WORKDIR /project

# 安装额外工具
RUN apt-get update && apt-get install -y \
    vim \
    nano \
    htop \
    git \
    && rm -rf /var/lib/apt/lists/*

# 设置环境变量
ENV IDF_PATH=/opt/esp/idf
ENV PATH="${IDF_PATH}/tools:${PATH}"

# 复制项目文件（构建时）
# COPY . /project

# 设置入口点
ENTRYPOINT ["/opt/esp/entrypoint.sh"]

# 默认命令
CMD ["/bin/bash"]

# ====================================================================
# 使用说明:
#
# 1. 构建镜像:
#    docker build -t esp32controlboard .
#
# 2. 运行容器（交互模式）:
#    docker run -it --rm -v $(pwd):/project esp32controlboard
#
# 3. 编译项目:
#    docker run --rm -v $(pwd):/project esp32controlboard idf.py build
#
# 4. 烧录固件（需要设备访问）:
#    docker run --rm --device=/dev/ttyUSB0 -v $(pwd):/project \
#      esp32controlboard idf.py -p /dev/ttyUSB0 flash
#
# 5. 完整流程:
#    docker run --rm --device=/dev/ttyUSB0 -v $(pwd):/project \
#      esp32controlboard idf.py build flash monitor
#
# macOS 用户注意:
# - 需要先安装 socat 来桥接 USB 设备
# - 或使用 Docker Desktop 的设备共享功能
# ====================================================================
