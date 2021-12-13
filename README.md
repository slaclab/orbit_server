# LCLS Orbit Server

This application monitors all LCLS BPMs, and emits a PV Access table of synchronized orbit data.

## To run:

	orbit_server [MODEL_PV] [EDEF] [OUTPUT_PV]

MODEL_PV: A PV to fetch the accelerator model from.  BMAD:SYS0:1:CU_HXR:LIVE:TWISS or BMAD:SYS0:1:CU_SXR:LIVE:TWISS will both work.  The orbit server only uses the device list from this PV, so the design versions work just as well.

EDEF: An event definition suffix to use.  Typically, this is either CUHBR or CUSBR, depending on which beam you are interested in.

OUTPUT_PV: The PV name to use for the orbit table.

## To build:

Requires an EPICS 7 environment in the $EPICS_BASE environment variable, $EPICS_HOST_ARCH set appropriately, and a version of GCC that supports C++11.

You'll need to build PVXS first.  Follow the instructions here: https://mdavidsaver.github.io/pvxs/building.html
To build in PVXS in prod at SLAC, you'll need to do these extra steps before building:
	cp pvxs-RELEASE.local pvxs-X.X.X/config/RELEASE.local
	cp pvxs-CONFIG_SITE pvxs-X.X.X/config/CONFIG_SITE
	ln -s pvxs-X.X.X pvxs
	make -C pvxs/bundle libevent
	# Finally, build as normal
	make -C pvxs

After all that's done, building orbit_server itself is pretty easy:

	make clean
	make -j8

## Acknowledgements

This project re-uses (and probably mangles) some code originally from https://github.com/slaclab/bsas.  Thanks to @mdavidsaver for writing it.