.section .text

# This code is used include the console.elf file for use
# in our process testing
_userelf: .incbin "../../user/console.elf"
_userelf_end:

userelf: .dword _userelf
userelfsize: .dword _userelf_end - _userelf
.global userelfsize
.global userelf
