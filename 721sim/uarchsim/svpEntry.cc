#include <inttypes.h>
#include <parameters.h>
#include <assert.h>

class svpEntry{

    public:

    uint64_t tag;
    uint64_t conf;
    uint64_t retVal; //potential to be discussed 
    int64_t stride; //potential to be discussed 
    uint64_t instance;
    bool valid;

    uint64_t getTag(){
        return this->tag;
    }

    void setTag(uint64_t tag){
        this->tag = tag;
    }

    void setInstanceCounter(uint64_t instanceCount){
        this->instance = instanceCount;
    }

    void increamentInstanceCounter(){
        this->instance++;
    }

    void decreamentInstanceCounter(){
        this->instance--;
        assert(this->instance >= 0 && "instace in svp became negative!!");
    }

    void increamentConf(){
        if(conf <= confMax) {
            this->conf = this->conf + confInc;
            if(this->conf > confMax)   
                this->conf = confMax; //saturate to confMax
        }
    }

    void decreamentConf(){
        if(confDec == 0)
            this->conf = 0;
        else if(confDec >0) {
            if(this->conf <= 0)
                this->conf = 0; //saturate to 0
            else
                this->conf = this->conf - confDec;   
        }
    }
};
