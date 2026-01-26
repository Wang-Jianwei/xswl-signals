#!/bin/bash

# 便捷编译脚本 - xswl-signals
# 使用方法: ./build.sh [command] [options]
# 命令: build, clean, rebuild, test, install, help

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
INSTALL_PREFIX="${SCRIPT_DIR}/install"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 打印信息
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

# 显示帮助信息
show_help() {
    cat << EOF
xswl-signals 编译脚本

使用方法:
  ./build.sh [command] [options]

命令:
  build       编译项目 (默认)
  clean       清空构建目录
  rebuild     重新编译项目
  test        运行测试
  install     安装到 $INSTALL_PREFIX
  help        显示此帮助信息

选项:
  --debug     Debug 编译模式 (默认为 Release)
  --install   编译后自动安装
  -j <n>      并行编译线程数 (默认为 CPU 核数)

示例:
  ./build.sh                    # 编译项目
  ./build.sh build --debug      # Debug 模式编译
  ./build.sh rebuild            # 清空并重新编译
  ./build.sh test               # 运行测试
  ./build.sh build --install    # 编译并安装

EOF
}

# 确保构建目录存在
setup_build_dir() {
    if [ ! -d "$BUILD_DIR" ]; then
        print_info "创建构建目录: $BUILD_DIR"
        mkdir -p "$BUILD_DIR"
    fi
}

# 编译
build() {
    local build_type="Release"
    local install_after=0
    local jobs=$(nproc)
    local i=1
    
    # 解析参数
    while [ $i -le $# ]; do
        arg=$(eval echo \$"$i")
        case $arg in
            --debug)
                build_type="Debug"
                ;;
            --install)
                install_after=1
                ;;
            -j)
                i=$((i+1))
                if [ $i -le $# ]; then
                    jobs=$(eval echo \$"$i")
                fi
                ;;
        esac
        i=$((i+1))
    done
    
    setup_build_dir
    
    print_info "开始编译 (模式: $build_type, 线程: $jobs)"
    
    cd "$BUILD_DIR"
    
    if [ ! -f "CMakeCache.txt" ]; then
        print_info "运行 CMake 配置..."
        cmake -DCMAKE_BUILD_TYPE="$build_type" \
              -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
              "$SCRIPT_DIR"
    fi
    
    print_info "编译中..."
    cmake --build . --parallel "$jobs"
    
    print_info "编译完成！"
    
    if [ $install_after -eq 1 ]; then
        install
    fi
}

# 清空
clean() {
    if [ -d "$BUILD_DIR" ]; then
        print_info "清空构建目录..."
        rm -rf "$BUILD_DIR"
        print_info "清空完成！"
    else
        print_warn "构建目录不存在"
    fi
}

# 重新编译
rebuild() {
    print_info "执行清空..."
    clean
    print_info "执行编译..."
    build "$@"
}

# 运行测试
run_tests() {
    if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
        print_error "项目未编译，请先运行 ./build.sh build"
        exit 1
    fi
    
    print_info "运行测试..."
    cd "$BUILD_DIR"
    ctest --output-on-failure
}

# 安装
install() {
    if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
        print_error "项目未编译，请先运行 ./build.sh build"
        exit 1
    fi
    
    print_info "安装到: $INSTALL_PREFIX"
    cd "$BUILD_DIR"
    cmake --install .
    print_info "安装完成！"
}

# 主程序逻辑
main() {
    local command="${1:-build}"
    shift || true
    
    case $command in
        build)
            build "$@"
            ;;
        clean)
            clean
            ;;
        rebuild)
            rebuild "$@"
            ;;
        test|tests)
            run_tests
            ;;
        install)
            install
            ;;
        help|-h|--help)
            show_help
            ;;
        *)
            print_error "未知命令: $command"
            show_help
            exit 1
            ;;
    esac
}

main "$@"
