/* The MIT License

   Copyright (c) 2013 Adrian Tan <atks@umich.edu>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include "program.h"

void VTOutput::failure(TCLAP::CmdLineInterface& c, TCLAP::ArgException& e)
{
    std::clog << "\n";
    std::clog << "  " << e.what() << "\n\n";
    usage(c);
    exit(1);
}

void VTOutput::usage(TCLAP::CmdLineInterface& c)
{
    std::string s = "";
    std::list<TCLAP::Arg*> args = c.getArgList();
    //prints unlabeled arument list first
    for (TCLAP::ArgListIterator it = args.begin(); it != args.end(); it++)
    {
        if (typeid(**it)==typeid(TCLAP::UnlabeledValueArg<std::string>))
        {
            TCLAP::UnlabeledValueArg<std::string> *i = (TCLAP::UnlabeledValueArg<std::string> *) (*it);
            s = i->getName();
        }
    }

    std::clog << c.getProgramName() << " v" << c.getVersion() << "\n\n";
    std::clog << "description : " << c.getMessage() << "\n\n";
    std::clog << "usage : vt "  << c.getProgramName() << " [options] " << s << "\n\n";

    //prints rest of arguments
    for (TCLAP::ArgListIterator it = args.begin(); it != args.end(); it++)
    {
        if (it==args.begin())
        {
            std::clog << "options : ";
        }
        else
        {
            std::clog << "          ";
        }

        if (typeid(**it)==typeid(TCLAP::ValueArg<std::string>) ||
            typeid(**it)==typeid(TCLAP::ValueArg<uint32_t>) ||
            typeid(**it)==typeid(TCLAP::ValueArg<int32_t>) ||
            typeid(**it)==typeid(TCLAP::ValueArg<double>))
        {
            TCLAP::ValueArg<std::string> *i = (TCLAP::ValueArg<std::string> *) (*it);

            std::clog  << "-" << i->getFlag()
                       << "  " << i->getDescription() << "\n";
        }
        else if (typeid(**it)==typeid(TCLAP::SwitchArg))
        {
            TCLAP::SwitchArg *i = (TCLAP::SwitchArg *) (*it);

            std::clog  << "-" << i->getFlag()
                       << "  " << i->getDescription() << "\n";
        }
        else if (typeid(**it)==typeid(TCLAP::UnlabeledValueArg<std::string>))
        {
            //ignored
        }
        else
        {
            std::clog << "oops, argument type not handled\n";
        }
    }

    std::clog  <<  "\n";
}

/**
 * Parse intervals. Processes the interval list first followed by the interval string. Duplicates are dropped.
 *
 * @intervals       - intervals stored in this vector
 * @interval_list   - file containing intervals
 * @interval_string - comma delimited intervals in a string
 *
 * todo: merge overlapping sites?
 */
void Program::parse_intervals(std::vector<GenomeInterval>& intervals, std::string interval_list, std::string interval_string)
{
    intervals.clear();
    std::map<std::string, uint32_t> m;

    if (interval_list!="")
    {
        htsFile *file = hts_open(interval_list.c_str(), "r");
        if (file)
        {
            kstring_t *s = &file->line;
            while (hts_getline(file, '\n', s)>=0)
            {
                std::string ss = std::string(s->s);
                if (m.find(ss)==m.end())
                {
                    m[ss] = 1;
                    GenomeInterval interval(ss);
                    intervals.push_back(interval);
                }
            }
            hts_close(file);
        }
    }

    std::vector<std::string> v;
    if (interval_string!="")
        split(v, ",", interval_string);

    for (uint32_t i=0; i<v.size(); ++i)
    {
        if (m.find(v[i])==m.end())
        {
            m[v[i]] = 1;
            GenomeInterval interval(v[i]);
            intervals.push_back(interval);
        }
    }
}

/**
 * Print reference FASTA file option.
 */
void Program::print_ref_op(const char* option_line, std::string ref_fasta_file)
{
    if (ref_fasta_file!="")
    {
        std::clog << option_line << ref_fasta_file << "\n";
    }
}

/**
 * Print intervals option.
 */
void Program::print_int_op(const char* option_line, std::vector<GenomeInterval>& intervals)
{
    if (intervals.size()!=0)
    {
        std::clog << option_line;
        for (uint32_t i=0; i<std::min((uint32_t)intervals.size(),(uint32_t)5); ++i)
        {
            if (i) std::clog << ",";
            std::clog << intervals[i].to_string();
        }
        if (intervals.size()>5)
        {
            std::clog << "  and " << (intervals.size()-5) <<  " other intervals\n";
        }
    }
}

/**
 * Parse samples. Processes the sample list. Duplicates are dropped.
 *
 * @samples      - samples stored in this vector
 * @sample_map   - samples stored in this map
 * @sample_list  - file containing sample names
 */
void Program::read_sample_list(std::vector<std::string>& samples, std::string sample_list)
{
    samples.clear();
    std::map<std::string, int32_t> map;

    if (sample_list!="")
    {
        htsFile *file = hts_open(sample_list.c_str(), "r");
        if (file)
        {
            kstring_t *s = &file->line;
            while (hts_getline(file, '\n', s)>=0)
            {
                std::string ss = std::string(s->s);
                if (map.find(ss)==map.end())
                {
                    map[ss] = 1;
                    samples.push_back(ss);
                }
            }
            hts_close(file);
        }
    }
}