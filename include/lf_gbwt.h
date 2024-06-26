/*
MIT License

Copyright (c) 2024 Ahsan Sanaullah
Copyright (c) 2024 S. Zhang Lab at UCF

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

//Classes in this header are derived from the GBWT and CompressedRecord, with the following license:
/*
Copyright (c) 2017, 2018, 2019, 2020, 2021, 2022, 2023, 2024 Jouni Siren
Copyright (c) 2015, 2016, 2017 Genome Research Ltd.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef GBWT_QUERY_LF_GBWT_H
#define GBWT_QUERY_LF_GBWT_H

//heavily derived/modified from FastLocate

#include<gbwt/gbwt.h>
#include<gbwt/internal.h>

namespace lf_gbwt{
    typedef std::uint8_t  byte_type;

    class GBWT;

    //all inputs and outputs to CompressedRecord are in the compressed alphabet (toComp)
    struct CompressedRecord
    {
        typedef gbwt::size_type size_type;

        //outgoing[i] stores the value BWT.rank(v,i) where v is the current node
        sdsl::int_vector<0> outgoing;

        //bit vector of length n where n is the length of the current node 
        //bit i is set if a concrete run starts at i
        sdsl::sd_vector<> first;

        //bit vector of length n*\sigma where \sigma is the size of outgoing 
        //bit i is set if a concrete run of value \floor(i/n) starts at i%n
        sdsl::sd_vector<> firstByAlphabet;

        //bit vector of length n where the ith set bit corresponds to the ith set bit in firstByAlphabet
        //the number of bits in [ith set bit, i+1th set bit) is the length of the concrete run corresponding to the
        //ith set bit
        sdsl::sd_vector<> firstByAlphComp;

        //bit vector of length gbwt.effective(), bit i is set if node i is contained in the BWT of this record
        sdsl::sd_vector<> alphabet;

        //int i stores the mapped alphabet value of the BWT run
        sdsl::int_vector<0> alphabetByRun;

        CompressedRecord() = default;
        CompressedRecord(const gbwt::CompressedRecord &, const GBWT *);

        size_type serialize(std::ostream& out, sdsl::structure_tree_node* v = nullptr, std::string name = "") const;
        void load(std::istream& in);

        size_type size() const { return this->first.size(); }
        bool empty() const { return (this->size() == 0); }
        std::pair<size_type, size_type> runs() const; // (concrete, logical)
        size_type outdegree() const { return this->outgoing.size(); }

        // Returns (node, LF(i, node)) or invalid_edge() if the offset is invalid.
        gbwt::edge_type LF(size_type i) const;

        // Returns `offset` such that `LF(offset) == (to, i)`, or `invalid_offset()`
        // if there is no such offset.
        // This can be used for computing inverse LF in a bidirectional GBWT.
        size_type offsetTo(gbwt::node_type to, size_type i) const;

        // As above, but also reports the closed offset range ('run') and the identifier
        // ('run_id') of the logical run used for computing LF().
        gbwt::edge_type LF(size_type i, gbwt::range_type& run, size_type& run_id) const;

        // As above, but also sets 'run_end' to the last offset of the current logical run.
        gbwt::edge_type runLF(size_type i, size_type& run_end) const;

        // Returns invalid_offset() if there is no edge to the destination.
        size_type LF(size_type i, gbwt::comp_type to) const;

        // Returns Range::empty_range() if the range is empty or the destination is invalid.
        gbwt::range_type LF(gbwt::range_type range, gbwt::comp_type to) const;

        // the following two functions assume valid i, 0 <= i < this->size()
        size_type compAlphabetAt(size_type i) const { return this->alphabetByRun[this->first.predecessor(i)->first]; }
        // Returns BWT[i] within the record
        gbwt::comp_type operator[](size_type i) const { return this->alphabet.select_iter(this->compAlphabetAt(i)+1)->second; }

        bool hasEdge(gbwt::comp_type to) const { return this->edgeTo(to) != gbwt::invalid_offset(); }

        // Maps successor nodes to outranks.
        gbwt::rank_type edgeTo(gbwt::comp_type to) const { 
            auto iter = this->alphabet.predecessor(to);
            return (iter != this->alphabet.one_end() && iter->second == to)? iter->first : gbwt::invalid_offset(); 
        };

        // These assume that 'outrank' is a valid outgoing edge.
        gbwt::comp_type successor(gbwt::rank_type outrank) const { return this->alphabet.select_iter(outrank+1)->second; }
        //gbwt::node_type successor(gbwt::rank_type outrank) const { return this->alphabet.successor(outrank)->second; }
        size_type offset(gbwt::rank_type outrank) const { return this->outgoing[outrank]; }

        //returns the run id by logical runs of an offset
        size_type logicalRunId(size_type i) const {
            if (i >= this->size())
                return gbwt::invalid_offset();
            size_type concRunId = this->first.predecessor(i)->first;
            if (!this->hasEdge(gbwt::ENDMARKER))
                return concRunId;
            size_type num0RunsBefore = this->firstByAlphabet.successor(i)->first;
            size_type num0Before = this->firstByAlphComp.select_iter(num0RunsBefore+1)->second;
            return concRunId - num0RunsBefore + num0Before;
        }
    };

    class GBWT
    {
        public:
            typedef CompressedRecord::size_type size_type;

            //------------------------------------------------------------------------------

            GBWT() = default;
            explicit GBWT(const gbwt::GBWT& source);

            size_type serialize(std::ostream& out, sdsl::structure_tree_node* v = nullptr, std::string name = "") const;
            void load(std::istream& in);
            
            const static std::string EXTENSION; // .lfgbwt

            //------------------------------------------------------------------------------

            /*
               Low-level interface: Statistics.
            */

            size_type size() const { return this->header.size; }
            bool empty() const { return (this->size() == 0); }
            size_type sequences() const { return this->header.sequences; }
            size_type sigma() const { return this->header.alphabet_size; }
            size_type effective() const { return this->header.alphabet_size - this->header.offset; }

            std::pair<size_type, size_type> runs() const;

            bool bidirectional() const { return this->header.get(gbwt::GBWTHeader::FLAG_BIDIRECTIONAL); }

            //------------------------------------------------------------------------------

            /*
               Metadata interface.
            */

            bool hasMetadata() const { return this->header.get(gbwt::GBWTHeader::FLAG_METADATA); }
            void addMetadata() { this->header.set(gbwt::GBWTHeader::FLAG_METADATA); }
            void clearMetadata() { this->metadata.clear(); this->header.unset(gbwt::GBWTHeader::FLAG_METADATA); };

            //------------------------------------------------------------------------------

            /*
               High-level interface. The queries check that the parameters are valid. Iterators
               must be InputIterators. On error or failed search, the return values will be the
               following:

               find     empty search state
               prefix   empty search state
               extend   empty search state
               locate   invalid_sequence() or empty vector
               extract  empty vector
            */

            gbwt::vector_type extract(size_type sequence) const;

            bool contains(gbwt::node_type node) const
            {
                return ((node < this->sigma() && node > this->header.offset) || node == gbwt::ENDMARKER);
            }

            gbwt::node_type firstNode() const { return this->header.offset + 1; }
            gbwt::comp_type toComp(gbwt::node_type node) const { return (node == 0 ? node : node - this->header.offset); }
            gbwt::node_type toNode(gbwt::comp_type comp) const { return (comp == 0 ? comp : comp + this->header.offset); }

            size_type nodeSize(gbwt::node_type node) const { return this->record(node).size(); }
            bool empty(gbwt::node_type node) const { return this->nodeSize(node) == 0; }

            //------------------------------------------------------------------------------

            /*
               Low-level interface: Navigation and searching. The interface assumes that node
               identifiers are valid. This can be checked with contains().
            */


            // On error: invalid_edge().
            gbwt::edge_type LF(gbwt::edge_type position) const
            {
                gbwt::edge_type ans = this->record(position.first).LF(position.second);
                ans.first = this->toNode(ans.first);
                return ans;
            }
            
            gbwt::edge_type LF(gbwt::edge_type position, gbwt::range_type& run, gbwt::size_type& run_id) const {
                gbwt::edge_type next = this->record(position.first).LF(position.second, run, run_id);
                next.first = this->toNode(next.first);
                return next;
            }

            // Only works in bidirectional indexes. May be slow when the predecessor is the endmarker.
            // On error: invalid_edge().
            gbwt::edge_type inverseLF(gbwt::node_type from, size_type i) const;

            // Only works in bidirectional indexes. May be slow when the predecessor is the endmarker.
            // On error: invalid_edge().
            gbwt::edge_type inverseLF(gbwt::edge_type position) const
            {
                return this->inverseLF(position.first, position.second);
            }
            
            gbwt::node_type predecessorAt(gbwt::node_type, size_type i) const;
            //------------------------------------------------------------------------------

            /*
               Low-level interface: Sequences. The interface assumes that node identifiers are
               valid. This can be checked with contains().
            */

            //------------------------------------------------------------------------------

            gbwt::GBWTHeader  header;
            gbwt::Tags        tags;
            std::vector<CompressedRecord> bwt;
            gbwt::Metadata    metadata;

            // Decompress and cache the endmarker, because decompressing it is expensive.
            //CompressedRecord *endmarker_record;

            //------------------------------------------------------------------------------

            /*
               Internal interface. Do not use.
            */


            const CompressedRecord& record(gbwt::node_type node) const { 
                if (!this->contains(node))
                    throw std::invalid_argument("node given to lf_gbwt.record invalid!");
                return this->bwt[this->toComp(node)]; 
            }

            bool verify(const gbwt::GBWT& g) {
                bool equal = (this->size() == g.size()) && (this->sequences() == g.sequences()) && (this->sigma() == g.sigma()) && (this->effective() == g.effective()) && (this->runs() == g.runs()) && (this->bidirectional() == g.bidirectional());
                equal = equal && this->verifyBWT(g);
                equal = equal && this->verifyLF(g);
                equal = equal && (!this->bidirectional() || this->verifyInverseLF(g));
                return equal;
            }

            //assumes number of nodes and node ids are equivalent
            bool verifyBWT(const gbwt::GBWT& g) {
                bool equal = true;
                #pragma omp parallel for schedule(dynamic, 1)
                for (gbwt::comp_type i = 0; i < this->effective(); ++i) {
                    gbwt::node_type node = this->toNode(i);
                    bool nodeEqual = node == g.toNode(i);
                    gbwt::CompressedRecord rec = g.record(node);
                    const CompressedRecord& lfrec = this->record(node);
                    nodeEqual = nodeEqual && rec.size() == lfrec.size();
                    for (gbwt::size_type j = 0; nodeEqual && j < lfrec.size(); ++j)
                        nodeEqual = nodeEqual && rec[j] == this->toNode(lfrec[j]);
                    #pragma omp critical 
                    {
                       equal = equal && nodeEqual;
                    }
                }
                return equal;
            }

            bool verifyLF(const gbwt::GBWT& g) {
                bool equal = true;
                #pragma omp parallel for schedule(dynamic, 1)
                for (gbwt::comp_type i = 0; i < this->effective(); ++i) {
                    gbwt::node_type node = this->toNode(i);
                    bool nodeEqual = node == g.toNode(i);
                    gbwt::size_type nodeSize = g.nodeSize(node);
                    nodeEqual = nodeEqual && nodeSize == this->nodeSize(node);
                    for (gbwt::size_type j = 0; nodeEqual && j < nodeSize; ++j)
                        nodeEqual = nodeEqual && this->LF({node, j}) == g.LF({node, j});
                    #pragma omp critical 
                    {
                       equal = equal && nodeEqual;
                    }
                }
                return equal;
            }

            bool verifyInverseLF(const gbwt::GBWT& g) {
                bool equal = this->bidirectional() && g.bidirectional();
                #pragma omp parallel for schedule(dynamic, 1)
                for (gbwt::comp_type i = 0; i < this->effective(); ++i) {
                    gbwt::node_type node = this->toNode(i);
                    bool nodeEqual = node == g.toNode(i);
                    gbwt::size_type nodeSize = g.nodeSize(node);
                    nodeEqual = nodeEqual && nodeSize == this->nodeSize(node);
                    for (gbwt::size_type j = 0; nodeEqual && j < nodeSize; ++j)
                        nodeEqual = nodeEqual && this->inverseLF({node, j}) == g.inverseLF({node, j});
                    #pragma omp critical 
                    {
                       equal = equal && nodeEqual;
                    }
                }
                return equal;
            }
    }; // class GBWT

    const std::string GBWT::EXTENSION = ".lfgbwt"; // .lfgbwt

    CompressedRecord::CompressedRecord(const gbwt::CompressedRecord & rec, const GBWT *source){
        size_type n = rec.size(), sigma = rec.outgoing.size(), runs = rec.runs().first;
        if (n == 0)
            return;

        //outgoing
        this->outgoing.resize(sigma);
        for (unsigned i = 0; i < sigma; ++i)
            this->outgoing[i] = rec.outgoing[i].second;
        sdsl::util::bit_compress(this->outgoing);

        //alphabet
        sdsl::sd_vector_builder alphabetBuilder(source->effective(), sigma);
        for (unsigned i = 0; i < sigma; ++i)
            alphabetBuilder.set(source->toComp(rec.outgoing[i].first));
        this->alphabet = sdsl::sd_vector<>(alphabetBuilder);

        //first, firstByAlphabet, firstByAlphComp, and alphabetByRun;
        sdsl::sd_vector_builder 
            firstBuilder(n, runs),
            firstByAlphabetBuilder(n*sigma, runs),
            firstByAlphCompBuilder(n, runs);
        this->alphabetByRun.resize(runs);

        //start location of run in firstByAlphabetAssist and length of run
        std::vector<std::vector<std::pair<size_type,size_type>>> firstByAlphabetAssist;
        firstByAlphabetAssist.resize(sigma);

        gbwt::CompressedRecordFullIterator iter(rec);
        size_type start, runInd = 0;
        while (!iter.end()){
            start = iter.offset() - iter.run.second;
            firstBuilder.set(start);
            firstByAlphabetAssist[iter.run.first].emplace_back(iter.run.first*n + start, iter.run.second);
            this->alphabetByRun[runInd] = iter.run.first;
            ++iter;
            ++runInd;
        }
        this->first = sdsl::sd_vector<>(firstBuilder);
        sdsl::util::bit_compress(this->alphabetByRun);

        size_type currInd = 0;
        for (const auto & alphArr : firstByAlphabetAssist){
            for (const auto & a : alphArr){
                firstByAlphabetBuilder.set(a.first);
                firstByAlphCompBuilder.set(currInd);
                currInd += a.second;
            }
        }
        this->firstByAlphabet = sdsl::sd_vector<>(firstByAlphabetBuilder);
        this->firstByAlphComp = sdsl::sd_vector<>(firstByAlphCompBuilder);
    }

    CompressedRecord::size_type CompressedRecord::serialize(std::ostream& out, sdsl::structure_tree_node* v, std::string name) const {
        sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, sdsl::util::class_name(*this));
        size_type written_bytes = 0;

        written_bytes += sdsl::serialize(this->outgoing, out, child, "outgoing");
        written_bytes += sdsl::serialize(this->first, out, child, "first");
        written_bytes += sdsl::serialize(this->firstByAlphabet, out, child, "firstByAlphabet");
        written_bytes += sdsl::serialize(this->firstByAlphComp, out, child, "firstByAlphComp");
        written_bytes += sdsl::serialize(this->alphabet, out, child, "alphabet");
        written_bytes += sdsl::serialize(this->alphabetByRun, out, child, "alphabetByRun");

        sdsl::structure_tree::add_size(child, written_bytes);
        return written_bytes;
    }

    void CompressedRecord::load(std::istream& in) {
        sdsl::load(this->outgoing, in);
        sdsl::load(this->first, in);
        sdsl::load(this->firstByAlphabet, in);
        sdsl::load(this->firstByAlphComp, in);
        sdsl::load(this->alphabet, in);
        sdsl::load(this->alphabetByRun, in);
    }

    std::pair<CompressedRecord::size_type, CompressedRecord::size_type> CompressedRecord::runs() const {
        size_type totRuns = this->first.ones();
        if (!this->hasEdge(gbwt::ENDMARKER)) 
            return {totRuns, totRuns};
        if (this->outgoing.size() == 1)
            return {totRuns, this->size()};
        size_type numENDMARKERruns = this->firstByAlphabet.predecessor(this->size()-1)->first + 1;
        size_type numENDMARKERS = this->firstByAlphComp.select_iter(numENDMARKERruns+1)->second;
        return {totRuns, totRuns - numENDMARKERruns + numENDMARKERS};
    }; // (concrete, logical)

    // Returns (node, LF(i, node)) or invalid_edge() if the offset is invalid.
    gbwt::edge_type CompressedRecord::LF(size_type i) const {
        if (i >= this->size())
            return gbwt::invalid_edge();
        gbwt::comp_type next = (*this)[i];
        return {next, this->LF(i, next)};
    }

    gbwt::node_type GBWT::predecessorAt(gbwt::node_type revFrom, size_type i) const {
        auto revNode = [this](gbwt::comp_type x) { 
            if (x == gbwt::ENDMARKER) 
                return x;
            return gbwt::Node::reverse(this->toNode(x));
        };
        const CompressedRecord & rev = this->record(revFrom);
        if (i >= rev.size())
            return gbwt::invalid_node();
        size_type predoutrank = rev.firstByAlphabet.select_iter(
                rev.firstByAlphComp.predecessor(i)->first+1
                )->second/rev.size();

        //check if before predoutrank is its reverse
        auto alphIter = rev.alphabet.select_iter(predoutrank+1);
        if (predoutrank > 0){
            auto prevAlphIter = --alphIter;
            ++alphIter;
            if (this->toNode(prevAlphIter->second) == revNode(alphIter->second)){
                size_type beforePrevAlph = rev.firstByAlphComp.select_iter(1+
                        rev.firstByAlphabet.successor((predoutrank-1)*rev.size())->first
                        )->second;
                size_type prevAlphSize = rev.firstByAlphComp.select_iter(1+
                        rev.firstByAlphabet.successor((predoutrank)*rev.size())->first
                        )->second;
                size_type AlphSize = rev.firstByAlphComp.select_iter(1+
                        rev.firstByAlphabet.successor((predoutrank+1)*rev.size())->first
                        )->second;
                AlphSize -= prevAlphSize;
                prevAlphSize -= beforePrevAlph;

                if (i < beforePrevAlph + AlphSize)
                    return revNode(alphIter->second);
                return this->toNode(alphIter->second);
            }
        }

        //check if after predoutrank is its reverse 
        if (predoutrank < rev.outgoing.size()-1){
            auto afterAlphIter = ++alphIter;
            --alphIter;
            if (this->toNode(afterAlphIter->second) == revNode(alphIter->second)){
                size_type beforeAlph = rev.firstByAlphComp.select_iter(1+
                        rev.firstByAlphabet.successor((predoutrank)*rev.size())->first
                        )->second;
                size_type AlphSize = rev.firstByAlphComp.select_iter(1+
                        rev.firstByAlphabet.successor((predoutrank+1)*rev.size())->first
                        )->second;
                size_type AfterAlphSize = rev.firstByAlphComp.select_iter(1+
                        rev.firstByAlphabet.successor((predoutrank+2)*rev.size())->first
                        )->second;
                AfterAlphSize -= AlphSize;
                AlphSize -= beforeAlph;

                if (i < beforeAlph + AfterAlphSize)
                    return revNode(afterAlphIter->second);
                return this->toNode(afterAlphIter->second);
            }
        }

        return revNode(alphIter->second);
    }
        

    // As above, but also reports the closed offset range ('run') and the identifier
    // ('run_id') of the logical run used for computing LF().
    gbwt::edge_type CompressedRecord::LF(size_type i, gbwt::range_type& run, size_type& run_id) const {
        if (i >= this->size()){
            run.first = run.second = gbwt::invalid_offset();
            run_id = gbwt::invalid_offset();
            return gbwt::invalid_edge();
        }
        run_id = this->logicalRunId(i);
        auto it = this->first.predecessor(i);
        run.first = it->second;
        ++it;
        run.second = it->second-1;

        return this->LF(i);
    }

    // Returns invalid_offset() if there is no edge to the destination.
    CompressedRecord::size_type CompressedRecord::LF(size_type i, gbwt::comp_type to) const {
        if (i > this->size() || !this->hasEdge(to))
            return gbwt::invalid_offset();
        size_type outrank = this->edgeTo(to);
        auto nextRun = this->firstByAlphabet.successor(outrank*this->size() + i);
        auto firstOutrankRun = this->firstByAlphabet.successor(outrank*this->size());
        auto nextRunComp = this->firstByAlphComp.select_iter(nextRun->first+1);
        auto firstOutrankRunComp = this->firstByAlphComp.select_iter(firstOutrankRun->first+1);
        size_type numOutrank = nextRunComp->second 
            - firstOutrankRunComp->second;
        if ((*this)[i] == to){
            size_type afteriInRun = this->first.successor(i)->second - i;
            numOutrank -= afteriInRun;
        }
        
        return this->outgoing[outrank] + numOutrank;
    }


    GBWT::GBWT(const gbwt::GBWT& source) {
        double start = gbwt::readTimer();
        this->header = source.header;
        this->tags = source.tags;
        this->metadata = source.metadata;

        double bwtStart = gbwt::readTimer();
        if(gbwt::Verbosity::level >= gbwt::Verbosity::BASIC)
        {
            std::cerr << "lf_GBWT::GBWT::GBWT(): Constructing " << source.effective() << " nodes of total length " << this->size() << std::endl;
        }
        this->bwt.resize(source.effective());
        source.bwt.forEach([this] (size_type a, const gbwt::CompressedRecord& rec) {this->bwt[a] = CompressedRecord(rec, this);});
        if(gbwt::Verbosity::level >= gbwt::Verbosity::BASIC)
        {
            double seconds = gbwt::readTimer() - bwtStart;
            std::cerr << "lf_GBWT::GBWT::GBWT(): Constructed " << source.effective() << " nodes of total length " << this->size() << " in " << seconds << " seconds" << std::endl;
        }

        //this->bwt.shrink_to_fit();
        if(gbwt::Verbosity::level >= gbwt::Verbosity::BASIC)
        {
            double seconds = gbwt::readTimer() - start;
            std::cerr << "lf_GBWT::GBWT::GBWT(): Processed " << source.effective() << " nodes of total length " << this->size() << " in " << seconds << " seconds" << std::endl;
        }
    }

    GBWT::size_type GBWT::serialize(std::ostream& out, sdsl::structure_tree_node* v, std::string name) const {
        sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, sdsl::util::class_name(*this));
        size_type written_bytes = 0;

        written_bytes += sdsl::serialize(this->header, out, child, "header");
        written_bytes += sdsl::serialize(this->tags, out, child, "tags");
        written_bytes += sdsl::serialize(this->bwt, out, child, "bwt");
        if(this->hasMetadata())
        {
            written_bytes += sdsl::serialize(this->metadata, out, child, "metadata");
        }
        
        sdsl::structure_tree::add_size(child, written_bytes);
        return written_bytes;
    }

    void GBWT::load(std::istream& in){
        sdsl::load(this->header, in);
        sdsl::load(this->tags, in);
        sdsl::load(this->bwt, in);
        if(this->hasMetadata())
        {
            sdsl::load(this->metadata, in);
        }
    }

    std::pair<gbwt::size_type, gbwt::size_type> GBWT::runs() const {
        std::pair<size_type, size_type> result(0,0);
        for (size_type i = 0; i < this->effective(); ++i){
            std::pair<size_type, size_type> temp = this->record(this->toNode(i)).runs();
            result.first  += temp.first; 
            result.second += temp.second;
        }
        return result;
    }

    gbwt::vector_type GBWT::extract(size_type sequence) const {
        gbwt::vector_type ans;
        if (sequence >= this->sequences())
            return ans;
        gbwt::edge_type position = {gbwt::ENDMARKER, sequence};
        position = this->LF(position);
        while (position.first != gbwt::ENDMARKER){
            ans.push_back(position.first);
            position = this->LF(position);
        }
        ans.shrink_to_fit();
        return ans;
    }

    gbwt::edge_type GBWT::inverseLF(gbwt::node_type from, size_type i) const {
        if (!this->bidirectional() || from == gbwt::ENDMARKER) { return gbwt::invalid_edge(); }

        //find the predecessor node id
        gbwt::node_type revFrom = gbwt::Node::reverse(from);
        gbwt::node_type pred = this->predecessorAt(revFrom, i);
        if (pred == gbwt::invalid_node()) { return gbwt::invalid_edge(); }

        //determine the offset 
        size_type offset = this->record(pred).offsetTo(this->toComp(from), i);
        if (offset == gbwt::invalid_offset()) { return {pred, offset}; return gbwt::invalid_edge(); }

        return { pred, offset};
    }

    CompressedRecord::size_type CompressedRecord::offsetTo(gbwt::comp_type to, size_type i) const {
        size_type outrank = this->edgeTo(to);
        if (outrank >= this->outdegree() || i < outgoing[outrank]) { return gbwt::invalid_offset(); }
        i -= outgoing[outrank];
        auto firstToRun = this->firstByAlphabet.successor(outrank*this->size());
        auto firstToCompRun = this->firstByAlphComp.select_iter(firstToRun->first+1);
        auto runComp = this->firstByAlphComp.predecessor(firstToCompRun->second + i);
        i -= (runComp->second - firstToCompRun->second);
        auto run = this->firstByAlphabet.select_iter(runComp->first+1);
        if (run->second/this->size() != outrank)
            return gbwt::invalid_offset();
        return (run->second-outrank*this->size()) + i;
    }
    
} // namespace lf_gbwt
#endif //GBWT_QUERY_LF_GBWT_H

