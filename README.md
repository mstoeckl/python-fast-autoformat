# PFA (Python Fast Autoformat)

An autoformatter should be three things:

* **Fast**: Run quickly so that you can afford to run the autoformatter on a large code base without ever having to wait for the autoformat to complete.
* **Consistent**: Running the autoformatter a second time won't change the output. Code will run the same before and after the autoformatter is run. Trivial whitespace changes will be reverted.
* **Pretty**: Output looks nice and preferably follows PEP8. 

PFA chooses to be *Fast* and *Consistent*, but sacrifices *Pretty* output in favor of speed.

## Installation and Usage

To create the executable file `pfa`, run

    make

Afterwards you can copy it into `PATH`, say to `/usr/bin` or `~/bin/`.

There are two ways to run the program. If the executable file name does not end in "i", i.e. with

    pfa that_python_script.py

then the formatted file will be dumped to standard output. If you make a symlink from `pfa` to `pfai` and run the latter like

    pfai that_python_script.py scriptus_secundus.py
    
then all files listed as arguments will be formatted in place.

## FAQ

* **Why is PFA written in C?** The startup time for the Python interpreter is often longer than it takes to run `pfa` on a 2000 line file.

* **How fast is it?** The other popular Python formatters are `yapf` and `autopep8`. Formatting about 120KB of python code in place for the second time in a row with the following commands,

        yapf -i bx.py
        autopep8 -i bx.py
        pfai bx.py

    one finds that `yapf` takes 14.0 seconds; `autopep8` takes 1.8 seconds; and `pfai` completes in 0.020 seconds, less than the time it takes to press Enter.

* **I have a change to contribute. Will it be accepted?**: Yes, as long as `pfa` still runs in O(n) for even pathological input.
