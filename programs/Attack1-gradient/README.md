# How to simulate rowhammer attack
To perform/simulate the regular physical attack, the points are as follows:
1.Identify the critical data: learning rate and epoch time N.
2.Analyze the .s to find the program would read these data from the static area.
3.Then manully modify the data read from the memory.

Therefore, to simplize the attack mechanism, only the critical segments are reached, we directly modify every data read from the memory, if we modify N times then we detect N times successfully, which is still enough to demonstrate the security.

Following is the instruction about how to build the execution file.

# Linear Regression using Gradient Descent in C 

A from scratch implementation of Linear Regression algorithm with 2 fitting parameters. The parameters are optimised using Gradient Descent algorithm. The implementation also utilises GNUplot to plot the data and cost over epochs.

## Getting Started

- Clone the repo into your folder

    `git clone https://github.com/sanjeev309/linear_regression_using_GD`

- Experiment with values of EPOCH, LEARNING_RATE and default theta values in main.cpp
- Execute make command to compile into an executable

    `make compile`

- Run the executable

    `./linear_reg`

- Clean for a rebuild

    `make clean`

### Prerequisites

*nix OS, gcc compiler, basic C programming and curiosity.

### Breakout

![Gradient Descent Algorithm for LR](https://cdn-images-1.medium.com/max/1600/1*o95nDGY2oV9r3jKLTRrIxw.png)


## Authors

* **Sanjeev Tripathi** - [LinkedIn](https://www.linkedin.com/in/sanjeev309/)


## License

This project is licensed under the MIT License - see the [LICENSE](https://github.com/sanjeev309/linear_regression_using_GD/blob/master/LICENSE.md) file for details

## Acknowledgments

* This project dates back to 3+ years when I just began with Andrew Ng's ML course in MATLAB. A lot of gratitude for him and the initiative.
