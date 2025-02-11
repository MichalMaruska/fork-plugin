
powershell "Import-Certificate -FilePath ^"\\precision40\public\kbfiltr.cer^" -CertStoreLocation cert:\CurrentUser\Root"



Import-Certificate -FilePath ".\kbfiltr.cer" -CertStoreLocation cert:\CurrentUser\Root


dir cert:\\LocalMachine\
