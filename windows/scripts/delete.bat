%
%
pnputil /delete-driver oem6.inf
if "false"=="true"  (
pnputil /delete-driver oem8.inf
pnputil /delete-driver oem9.inf
pnputil /delete-driver oem10.inf
pnputil /delete-driver oem11.inf
pnputil /delete-driver oem12.inf
pnputil /delete-driver oem13.inf

pnputil /delete-driver oem14.inf
pnputil /delete-driver oem15.inf
pnputil /delete-driver oem16.inf
pnputil /delete-driver oem17.inf
pnputil /delete-driver oem18.inf
pnputil /delete-driver oem19.inf

pnputil /delete-driver oem20.inf
pnputil /delete-driver oem21.inf
pnputil /delete-driver oem22.inf
pnputil /delete-driver oem23.inf
pnputil /delete-driver oem24.inf
pnputil /delete-driver oem25.inf
pnputil /delete-driver oem26.inf
pnputil /delete-driver oem27.inf
pnputil /delete-driver oem28.inf
pnputil /delete-driver oem29.inf

pnputil /enum-devicetree /connected /interfaces

pnputil /enum-devices /class Keyboard

rem pnputil /enable-device


pnputil /enum-classes /class Keyboard

pnputil /enum-interfaces
pnputil /enum-containers

)

pnputil /enum-drivers /class Keyboard


