#!/bin/bash

set -e
sudo apt-get install -y g++ nasm qemu-system-x86 grub-pc-bin xorriso mtools

# Create build directory
mkdir -p build

# Compile the assembly bootloader
echo "Compiling boot.asm..."
nasm -f elf32 boot.asm -o build/boot.o

# Compile the kernel 
# Added -fno-use-cxa-atexit to fix '__cxa_atexit' and '__dso_handle' undefined references
echo "Compiling kernel.cpp..."
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-use-cxa-atexit -O2 -c kernel.cpp -o build/kernel.o

# Link the kernel
# Added -z noexecstack to silence the .note.GNU-stack warning
echo "Linking kernel..."
ld -m elf_i386 -z noexecstack -T linker.ld -o build/kernel.bin build/boot.o build/kernel.o

# Check if kernel.bin exists and has size greater than 0
if [ ! -s build/kernel.bin ]; then
    echo "Error: kernel.bin is empty or does not exist."
    exit 1
fi

# Create a bootable ISO
echo "Creating bootable ISO..."
mkdir -p build/iso/boot/grub
cp build/kernel.bin build/iso/boot/
cat > build/iso/boot/grub/grub.cfg << EOF
set timeout=0
set default=0

menuentry "Cinemint OS" {
    multiboot /boot/kernel.bin
    boot
}
EOF

grub-mkrescue -o build/cos.iso build/iso

# Run the OS in QEMU
echo "Launching QEMU..."
qemu-system-i386 -cdrom build/cos.iso