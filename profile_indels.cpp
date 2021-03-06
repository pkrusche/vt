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

#include "profile_indels.h"

namespace
{
class OverlapStats
{
    public:

    uint32_t a,ab,b,a_ins,ab_ins,b_ins,a_del,ab_del,b_del;

    OverlapStats()
    {
        a = 0;
        ab = 0;
        b = 0;

        a_ins = 0;
        a_del = 0;
        ab_ins = 0;
        ab_del = 0;
        b_ins = 0;
        b_del = 0;
    };
};

class Igor : Program
{
    public:

    std::string version;

    ///////////
    //options//
    ///////////
    std::string input_vcf_file;
    std::vector<std::string> input_vcf_files;
    std::string ref_fasta_file;
    std::string ref_data_sets_list;
    std::vector<GenomeInterval> intervals;
    std::string interval_list;

    std::vector<std::string> dataset_labels;
    std::vector<std::string> dataset_types;

    std::vector<OverlapStats> stats;

    std::string gencode_gtf_file;


    ///////
    //i/o//
    ///////
    BCFSyncedReader *sr;
    bcf1_t *v;
    Filter *filter;
    kstring_t line;
    Filter *rare_filter;

    /////////
    //stats//
    /////////
    uint32_t no_indels;
    uint32_t nfs;
    uint32_t fs;
    uint32_t rare_nfs;
    uint32_t rare_fs;
    uint32_t common_nfs;
    uint32_t common_fs;

    ////////////////
    //common tools//
    ////////////////
    VariantManip *vm;
    GENCODE *gc;

    Igor(int argc, char ** argv)
    {
        //////////////////////////
        //options initialization//
        //////////////////////////
        try
        {
            std::string desc = "profile Indels";

            version = "0.5";
            TCLAP::CmdLine cmd(desc, ' ', version);
            VTOutput my; cmd.setOutput(&my);
            TCLAP::ValueArg<std::string> arg_ref_fasta_file("r", "r", "reference sequence fasta file []", true, "", "str", cmd);
            TCLAP::ValueArg<std::string> arg_intervals("i", "i", "intervals []", false, "", "str", cmd);
            TCLAP::ValueArg<std::string> arg_interval_list("I", "I", "file containing list of intervals []", false, "", "file", cmd);
            TCLAP::ValueArg<std::string> arg_ref_data_sets_list("g", "g", "file containing list of reference datasets []", false, "", "file", cmd);
            TCLAP::UnlabeledValueArg<std::string> arg_input_vcf_file("<in.vcf>", "input VCF file", true, "","file", cmd);

            cmd.parse(argc, argv);

            ref_fasta_file = arg_ref_fasta_file.getValue();
            parse_intervals(intervals, arg_interval_list.getValue(), arg_intervals.getValue());
            ref_data_sets_list = arg_ref_data_sets_list.getValue();
            input_vcf_file = arg_input_vcf_file.getValue();

            ///////////////////////
            //parse input VCF files
            ///////////////////////
        }
        catch (TCLAP::ArgException &e)
        {
            std::cerr << "error: " << e.error() << " for arg " << e.argId() << "\n";
            abort();
        }
    };

    void initialize()
    {
        //////////////////////
        //reference data set//
        //////////////////////
//# This file contains information on how to process reference data sets.
//#
//# dataset - name of data set, this label will be printed.
//# type    - True Positives (TP) and False Positives (FP)
//#           overlap percentages labeled as (Precision, Sensitivity) and (False Discovery Rate, Type I Error) respectively
//#         - annotation
//#           file is used for GENCODE annotation of frame shift and non frame shift Indels
//# filter  - filter applied to variants for this particular data set
//# path    - path of indexed BCF file
//#dataset              type         filter           path
//mills                 TP           INDEL            /net/fantasia/home/atks/dev/vt/ftp/grch37/mills.208620indels.sites.bcf
//mills.chip            TP           INDEL            /net/fantasia/home/atks/dev/vt/ftp/grch37/mills.chip.158samples.8904indels.sites.bcf
//mills.chip.common     TP           INDEL&&AF>0.005  /net/fantasia/home/atks/dev/vt/ftp/grch37/mills.chip.158samples.8904indels.sites.bcf
//affy.exome.chip       TP           INDEL            /net/fantasia/home/atks/dev/vt/ftp/grch37/affy.exome.chip.1249samples.316520variants.sites.bcf
//affy.exome.chip.poly  TP           INDEL&&AC!=0     /net/fantasia/home/atks/dev/vt/ftp/grch37/affy.exome.chip.1249samples.316520variants.sites.bcf
//affy.exome.chip.mono  FP           INDEL&&AC=0      /net/fantasia/home/atks/dev/vt/ftp/grch37/affy.exome.chip.1249samples.316520variants.sites.bcf
//gencode.v19           annotation   .                /net/fantasia/home/atks/dev/vt/ftp/grch37/gencode.v19.annotation.gtf.gz

        input_vcf_files.push_back(input_vcf_file);
        dataset_labels.push_back("data");
        dataset_types.push_back("ref");

        htsFile *hts = hts_open(ref_data_sets_list.c_str(), "r");
        kstring_t s = {0,0,0};
        std::vector<std::string> vec;
        while (hts_getline(hts, '\n', &s)>=0)
        {
            if (s.s[0] == '#')
                continue;

            std::string line(s.s);
            split(vec, " ", line);

            if (vec[1] == "TP" || vec[1] == "FP")
            {
                dataset_labels.push_back(vec[0]);
                dataset_types.push_back(vec[1]);
                input_vcf_files.push_back(vec[3]);
            }
            else if (vec[1] == "annotation")
            {
                gencode_gtf_file = vec[3];
            }
            else
            {
                std::cerr << "Reference data set type: \"" << vec[1] << "\" not recognised\n";
                exit(1);
            }
        }
        hts_close(hts);
        if (s.m) free(s.s);

        //////////////////////
        //i/o initialization//
        //////////////////////
        sr = new BCFSyncedReader(input_vcf_files, intervals, false);

        ///////////////////////
        //tool initialization//
        ///////////////////////
        vm = new VariantManip(ref_fasta_file);
        gc = new GENCODE(gencode_gtf_file, ref_fasta_file);

        ////////////////////////
        //stats initialization//
        ////////////////////////
        no_indels = 0;
        fs = 0;
        nfs = 0;
    }

    void profile_indels()
    {
        //for combining the alleles
        std::vector<bcfptr*> current_recs;
        std::vector<Interval*> overlaps;
        Variant variant;
        int32_t no_overlap_files = input_vcf_files.size();
        std::vector<int32_t> presence(no_overlap_files, 0);
        stats.resize(no_overlap_files);

        while(sr->read_next_position(current_recs))
        {
            //check first variant
            bcf1_t *v = current_recs[0]->v;
            bcf_hdr_t *h = current_recs[0]->h;
            int32_t vtype = vm->classify_variant(h, v, variant);

            if (!((vtype==VT_INDEL || vtype==(VT_SNP|VT_INDEL)) && bcf_get_n_allele(v)==2))
            {
                continue;
            }

            std::string chrom = bcf_get_chrom(h,v);
            int32_t start1 = bcf_get_pos1(v);
            int32_t end1 = bcf_get_end_pos1(v);

            //check existence
            for (uint32_t i=0; i<current_recs.size(); ++i)
            {
                ++presence[current_recs[i]->file_index];
            }

            //annotate
            if (presence[0])
            {
                gc->search(chrom, start1+1, end1, overlaps);

                bool cds_found = false;
                bool is_fs = false;
                    
                for (int32_t i=0; i<overlaps.size(); ++i)
                {
                    GENCODERecord *rec = (GENCODERecord *) overlaps[i];
                    if (rec->feature==GC_FT_CDS)
                    {
                        cds_found = true;
                        if (abs(variant.alleles[0].dlen)%3!=0)
                        {
                            is_fs = true;
                            break;
                        }
                    }
                }

                if (cds_found)
                {
                    if (is_fs)
                    {
                        ++fs;
                    }
                    else
                    {
                        ++nfs;
                    }
                }

                ++no_indels;
            }

            int32_t ins = variant.alleles[0].ins;
            int32_t del = 1-ins;

            if (presence[0])
            {
                ++stats[0].a;
                stats[0].a_ins += ins;
                stats[0].a_del += del;
            }

            //update overlap stats
            for (uint32_t i=1; i<no_overlap_files; ++i)
            {
                if (presence[0] && !presence[i])
                {
                    ++stats[i].a;
                    stats[i].a_ins += ins;
                    stats[i].a_del += del;
                }
                else if (presence[0] && presence[i])
                {
                    ++stats[i].ab;
                    stats[i].ab_ins += ins;
                    stats[i].ab_del += del;
                }
                else if (!presence[0] && presence[i])
                {
                    ++stats[i].b;
                    stats[i].b_ins += ins;
                    stats[i].b_del += del;
                }
                else
                {
                    //not in either, do nothing
                }

                presence[i]=0;
            }

            presence[0] = 0;
        }
    };

    void print_options()
    {
        std::clog << "profile_indels v" << version << "\n\n";
        std::clog << "\n";
        std::clog << "Options:     input VCF File                 " << input_vcf_file << "\n";
        std::clog << "         [g] reference data sets list file  " << ref_data_sets_list << "\n";
        std::clog << "         [r] reference FASTA file           " << ref_fasta_file << "\n";
        print_int_op("         [i] intervals                      ", intervals);
        std::clog << "\n";
   }

    void print_stats()
    {
        fprintf(stderr, "\n");
        fprintf(stderr, "  %s\n", "data set");
        fprintf(stderr, "    No Indels : %10d [%.2f]\n", stats[0].a,  (float)stats[0].a_ins/(stats[0].a_del));
        fprintf(stderr, "       FS/NFS : %10.2f (%d/%d)\n", (float)fs/(fs+nfs), fs, nfs);
        fprintf(stderr, "\n");

        for (int32_t i=1; i<dataset_labels.size(); ++i)
        {
            fprintf(stderr, "  %s\n", dataset_labels[i].c_str());
            fprintf(stderr, "    A-B %10d [%.2f]\n", stats[i].a,  (float)stats[i].a_ins/(stats[i].a_del));
            fprintf(stderr, "    A&B %10d [%.2f]\n", stats[i].ab, (float)stats[i].ab_ins/stats[i].ab_del);
            fprintf(stderr, "    B-A %10d [%.2f]\n", stats[i].b,  (float)stats[i].b_ins/(stats[i].b_del));

            if (dataset_types[i]=="TP")
            {
                fprintf(stderr, "    Precision    %4.1f%%\n", 100*(float)stats[i].ab/(stats[i].a+stats[i].ab));
                fprintf(stderr, "    Sensitivity  %4.1f%%\n", 100*(float)stats[i].ab/(stats[i].b+stats[i].ab));
            }
            else
            {
                fprintf(stderr, "    FDR          %4.1f%%\n", 100*(float)stats[i].ab/(stats[i].a+stats[i].ab));
                fprintf(stderr, "    Type I Error %4.1f%%\n", 100*(float)stats[i].ab/(stats[i].b+stats[i].ab));
            }
            fprintf(stderr, "\n");
        }
    };

    ~Igor()
    {
    };

    private:
};

}

void profile_indels(int argc, char ** argv)
{
    Igor igor(argc, argv);
    igor.print_options();
    igor.initialize();
    igor.profile_indels();
    igor.print_stats();
}
