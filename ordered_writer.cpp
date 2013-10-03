/* The MIT License

   Copyright (c) 2008 Broad Institute / Massachusetts Institute of Technology
                 2011, 2012 Attractive Chaos <attractor@live.co.uk>

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

#include "ordered_writer.h"

OrderedWriter::OrderedWriter(std::string _vcf_file, int32_t _window)
{
    vcf_file = _vcf_file;
    window = _window;
    vcf = NULL;
    hdr = NULL;
    
    ss.str("");
    s.s = 0; s.l = s.m = 0;
    
    vcf = vcf_open(vcf_file.c_str(), modify_mode(vcf_file.c_str(), 'w'), 0);
}

/**
 * Gets sequence name of a record
 */
const char* OrderedWriter::get_seqname(bcf1_t *v)
{
    return bcf_get_chrom(hdr, v);
}                   

/**
 * Gets record from pool, creates a new record if necessary
 */
void OrderedWriter::set_hdr(bcf_hdr_t *_hdr)
{
    hdr = _hdr;
}

/**
 * Gets record from pool, creates a new record if necessary
 */
bcf1_t* OrderedWriter::get_bcf1_from_pool()
{    
    if(!pool.empty())
    {
        bcf1_t* v = pool.front();
        pool.pop_front();
        return v;
    }
    else
    {
        return bcf_init1(); 
    }
};

/**
 * Returns record to pool 
 */ 
void OrderedWriter::store_bcf1_into_pool(bcf1_t* v)
{
    pool.push_back(v);
}

/**
 * Reads next record, hides the random access of different regions from the user.
 */
void OrderedWriter::write1(bcf1_t *v)
{   
    //place into appropriate position in the buffer
    if (!buffer.empty())
    {
        //same chromosome?
        if (bcf_get_rid(v)==bcf_get_rid(buffer.back()))
        {
            std::list<bcf1_t*>::iterator i;
            for (i=buffer.begin(); i!=buffer.end(); ++i)
            {
                //equal sign ensures records are kept in original order
                if (bcf_get_pos1(v)>=bcf_get_pos1(*i))
                {
                    buffer.insert(i,v);
                    flush(false);
                    return;
                }
            }
            
            //check order
            if (i==buffer.end())
            {
                int32_t cutoff_pos1 =  std::max(bcf_get_pos1(buffer.front())-window,1); 
                if (bcf_get_pos1(buffer.back())<cutoff_pos1)
                {
                     std::cerr << "Might not be sorted\n";
                }
            }
            
            buffer.insert(i,v);
            flush(false);  
        }
        else
        {
            flush(true);            
            buffer.push_front(v);
        }
    }    
    else
    {
        buffer.push_front(v);   
    }
}

/**
 * Reads next record, hides the random access of different regions from the user.
 */
void OrderedWriter::write_hdr()
{   
    vcf_hdr_write(vcf, hdr);
}

/**
 * Flush writable records from buffer.
 */
void OrderedWriter::flush()
{    
    flush(true);
}

/**
 * Flush writable records from buffer.
 */
void OrderedWriter::flush(bool force)
{    
    if (force)
    {
        while (!buffer.empty())
        {
            vcf_write1(vcf, hdr, buffer.back());
            store_bcf1_into_pool(buffer.back());
            buffer.pop_back();
        }
    }    
    else
    {
        if (buffer.size()>=2)
        {   
            int32_t cutoff_pos1 =  std::max(bcf_get_pos1(buffer.front())-window,1); 
            
            while (buffer.size()>1)
            {
                if (bcf_get_pos1(buffer.back())<cutoff_pos1)
                {
                    vcf_write1(vcf, hdr, buffer.back());
                    store_bcf1_into_pool(buffer.back());
                    buffer.pop_back();
                }
                else
                {
                    return;
                }
            }
        }
    }
}