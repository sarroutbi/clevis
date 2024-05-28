pcscd = find_program('pcscd', required: false)
pkcs11tool = find_program('pkcs11-tool', required: false)
### TODO: Check if we can remove pkcs15-tool dependency and reuse pkcs11-tool
pkcs15tool = find_program('pkcs15-tool', required: false)
pcscd_disable_polkit = false

if pcscd.found()
  pcscd_options = run_command('pcscd', '--help', check: false)
  pcscd_disable_polkit = pcscd_options.stdout().strip().contains('disable-polkit')
endif

if pcscd.found() and pkcs11tool.found() and pkcs15tool.found() and pcscd_disable_polkit
  bins += join_paths(meson.current_source_dir(), 'clevis-decrypt-pkcs11')
  bins += join_paths(meson.current_source_dir(), 'clevis-encrypt-pkcs11')
  ### TODO: Include man pages
  # mans += join_paths(meson.current_source_dir(), 'clevis-encrypt-pkcs11.1')
  # mans += join_paths(meson.current_source_dir(), 'clevis-decrypt-pkcs11.1')
  subdir('tests')
  executable('clevis-pkcs11-afunix-socket-unlock', ['clevis-pkcs11-afunix-socket-unlock.c'],
    install_dir: bindir,
    install: true,
  )
else
  warning('Will not install pkcs11 pin due to missing dependencies!')
  if not pcscd.found()
    warning('pcscd not found')
  elif not pcscd_disable_polkit
    warning('pcscd does not have --disable-polkit option')
  endif
  if not pkcs11tool.found()
    warning('pkcs11-tool not found')
  endif
  if not pkcs15tool.found()
    warning('pkcs15-tool not found')
  endif
endif
