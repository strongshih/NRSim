import torch
import torch.nn as nn
import torch.optim as optim
import torch.ao.quantization as quantization
import random
import numpy as np

# Set random seeds for reproducibility
seed = 42
random.seed(seed)
np.random.seed(seed)
torch.manual_seed(seed)
torch.cuda.manual_seed(seed)
torch.backends.cudnn.deterministic = True
torch.backends.cudnn.benchmark = False

# Define the simple neural network
class SimpleNet(nn.Module):
    def __init__(self):
        super(SimpleNet, self).__init__()
        self.fc1 = nn.Linear(256, 256)
        self.fc2 = nn.Linear(256, 4)
        self.relu = nn.ReLU()
        self.quant = quantization.QuantStub()
        self.dequant = quantization.DeQuantStub()

    def forward(self, x):
        x = self.quant(x)
        print(x[0,...])
        x = self.fc1(x)
        print(x[0,...], x[0,...].shape)
        x = self.relu(x)
        x = self.fc2(x)
        print(x[0,...], x[0,...].shape)
        x = self.dequant(x)
        return x

# Initialize the network
model = SimpleNet()

# To make FixedQParamsObserver work
# https://github.com/pytorch/pytorch/blob/main/torch/ao/quantization/observer.py
# /home/lindyred/miniconda3/lib/python3.12/site-packages/torch/ao/quantization/observer.py L63: self.p(*args)#, **keywords)
# (2.4.0+cu121)
model.qconfig = quantization.QConfig(
    activation=quantization.FixedQParamsObserver.with_args(scale=1.0 / 128.0, 
                                                           zero_point=128, 
                                                           dtype=torch.quint8, 
                                                           quant_min=0, 
                                                           quant_max=255),  
    weight=quantization.FixedQParamsObserver.with_args(scale=1.0 / 128.0, 
                                                       zero_point=0, 
                                                       dtype=torch.qint8, 
                                                       quant_min=-127, 
                                                       quant_max=127)
)
#model.qconfig = quantization.get_default_qat_qconfig('qnnpack')
model = quantization.prepare_qat(model)

# Create dummy dataset (input dimension is 256, output dimension is 4)
x_train = torch.randn(100, 256)
y_train = torch.randint(0, 4, (100,))

# Define loss function and optimizer
criterion = nn.CrossEntropyLoss()
optimizer = optim.SGD(model.parameters(), lr=0.01)

# Training loop
epochs = 10
for epoch in range(epochs):
    model.train()
    optimizer.zero_grad()
    
    # Forward pass
    outputs = model(x_train)
    loss = criterion(outputs, y_train)
    
    # Backward pass and optimization
    loss.backward()
    optimizer.step()

    print(f'Epoch [{epoch+1}/{epochs}], Loss: {loss.item():.4f}')

# Convert the trained model to quantized version
model.eval()
model = quantization.convert(model)

# Function to export weights and sample inputs to a .h file
def export_to_header(model, num_samples=192):
    with open("mlp_test.h", "w") as f:
        # Write weights for the first layer (256x256)
        fc1_weights_int = torch.int_repr(model.fc1.weight()).numpy()

        f.write("static float mlp0_weight[256][256] = {\n")
        for i in range(256):
            f.write("    {")
            float_weights = [(fc1_weights_int[i][j]) for j in range(256)]
            f.write(", ".join([f"{float_weights[j]:.6f}" for j in range(256)]))
            f.write("},\n")
        f.write("};\n\n")

        # Write bias for the first layer (256)
        if model.fc1.bias is not None:
            fc1_bias = model.fc1.bias().detach().numpy()
            fc1_bias = np.round(model.fc1.bias().detach().numpy() * 128)
            f.write("static float mlp0_bias[256] = \n")
            f.write("    {")
            f.write(", ".join([f"{fc1_bias[i]:.6f}" for i in range(256)]))
            f.write("};\n\n")

        # Write weights for the second layer (4x256)
        fc2_weights_int = torch.int_repr(model.fc2.weight()).numpy()

        f.write("static float mlp1_weight[4][256] = {\n")
        for i in range(4):
            f.write("    {")
            float_weights = [(fc2_weights_int[i][j]) for j in range(256)]
            f.write(", ".join([f"{float_weights[j]:.6f}" for j in range(256)]))
            f.write("},\n")
        f.write("};\n\n")
        
        # Write bias for the second layer (4)
        if model.fc2.bias is not None:
            fc2_bias = np.round(model.fc2.bias().detach().numpy() * 128)
            f.write("static float mlp1_bias[4] = \n")
            f.write("    {")
            f.write(", ".join([f"{fc2_bias[i]:.6f}" for i in range(4)]))
            f.write("};\n\n")

        # Generate and write sample inputs
        test_inputs = []
        f.write(f"static float test_input[{num_samples:4d}][256] = " + "{\n")
        for _ in range(num_samples):
            sample_input = np.random.rand(256).astype(np.float32)
            sample_input_int = (sample_input * 128).astype(np.int32)
            test_inputs.append(sample_input)
            f.write("    {")
            f.write(", ".join([f"{sample_input_int[j]:.6f}" for j in range(256)]))
            f.write("},\n")
        f.write("};\n\n")

        # Evaluate the model on the sample inputs and write the outputs (results) to the file
        f.write(f"static float test_output[{num_samples:4d}][4] = " + "{\n")
        test_inputs_tensor = torch.tensor(np.array(test_inputs))  # Convert to tensor for inference
        with torch.no_grad():
            model.eval()  # Ensure the model is in evaluation mode
            outputs = model(test_inputs_tensor)  # Get the model outputs
            outputs = outputs.detach().numpy()  # Convert outputs to numpy

            for i in range(num_samples):
                f.write("    {")
                f.write(", ".join([f"{outputs[i][j]:.6f}" for j in range(4)]))
                f.write("},\n")
        f.write("};\n")

# Export the weights and sample inputs to the header file
export_to_header(model)
print("Weights and sample inputs exported to mlp_test.h")

