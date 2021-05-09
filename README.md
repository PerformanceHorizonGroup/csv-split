# csv-split

csv-split is a simple utility that can parse and break up large CSV files into smaller pieces, with various options on how it does that.  

----
# Compiling
---

Clone the repository, cd into the directory, and run the following:

~~~
make && make install 
~~~

----
# Usage
----

csv-split [OPTIONS] FILE OUT-PATH

or

csv-split [OPTIONS] --stdin PREFIX OUT-PATH

*   **--g, --group-col**
    The zero based column with values that must remain together.  If specified, csv-split will not seperate
    rows with the same value in this column acros multiple files.  This assumes the file is already sorted
    by this colum, as csv-split doesn't sort the file.

*   **-n, --num-rows**
    The maximum number of rows to put in each file.  If we're grouping column values (see above), you can
    end up with files with slightly more rows

*   **--stdin**
    If specified, csv-split will read data from STDIN instead of a provided file, and the file argument
    will be treated as a prefix to use when writing output chunks.

*   **-t, --trigger**
    Each time csv-split writes a file, it can be configured to run a command specified by this option.
    Two environment variables will be set prior to the execution of the command:

        CSV-PAYLOAD_FILE -- The filename that was written
        CSV_ROWCOUNT     -- How many rows are in this file

*   **-d, --header**
    If you pass the --header option, csv-split will treat the first row of the input csv file as a header
    and inject it into each split file.  By default, the header row is not counted toward the total number
    of rows written per file, but can be counted if you pass 1 to this argument (e.g. -d1, --header=1).
