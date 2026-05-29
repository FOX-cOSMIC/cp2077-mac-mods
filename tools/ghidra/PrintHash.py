# Print the hash/identity of the binary Ghidra analyzed (read-only).
prog = getCurrentProgram()
print("GHIDRA_PROG_NAME: " + str(prog.getName()))
print("GHIDRA_EXEC_PATH: " + str(prog.getExecutablePath()))
print("GHIDRA_EXEC_SHA256: " + str(prog.getExecutableSHA256()))
print("GHIDRA_EXEC_MD5: " + str(prog.getExecutableMD5()))
print("GHIDRA_IMAGE_BASE: " + str(prog.getImageBase()))
print("GHIDRA_LANG: " + str(prog.getLanguageID()))
