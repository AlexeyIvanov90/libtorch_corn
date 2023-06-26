#include "model.h"
#include <chrono>



void siam_train(Siam_data_loader data_train, Siam_data_set data_val, std::string path_save_NN, int epochs, torch::Device device)
{
	if (device == torch::kCPU)
		std::cout << "Training on CPU" << std::endl;
	else
		std::cout << "Training on GPU" << std::endl;

	ConvNet model(3, 64, 64);
	model->to(device);

	torch::optim::Adam optimizer(model->parameters(), torch::optim::AdamOptions(1e-3));

	int64_t log_interval = 10;
	int dataset_size = data_train.size();

	float best_mse = std::numeric_limits<float>::max();

	model->train();

	for (int epoch = 1; epoch <= epochs; epoch++) {
		auto begin = std::chrono::steady_clock::now();

		float mse = 0.; // mean squared error
		int count = 0;

		size_t bath_counter = 0;

		for (; !data_train.epoch_end();) {

			std::string consol_text = "\r" + std::to_string((int)(((data_train.num_batch()) / ((float)dataset_size/ data_train.size_batch()))*100)) + "%";
			std::cout << consol_text;

			Batch data = data_train.get_batch();
			
			auto imgs_1 = data.img_1;
			auto imgs_2 = data.img_2;
			auto labels = data.label.squeeze();

			imgs_1 = imgs_1.to(device);
			imgs_2 = imgs_2.to(device);
			labels = labels.to(device);

			auto output = model->forward(imgs_1, imgs_2);

			//auto loss = (1 - labels[0]) * torch::pow(output[0], 2) + (labels[0])* torch::pow(torch::clamp(1.0 - output[0], 0.0), 2);
			//std::cout << "loss\n" << loss << std::endl;


			//for (int obj = 1; obj < labels.sizes()[0]; obj++) {
			//	auto tensor_buf = (1 - labels[obj]) * torch::pow(output[obj], 2) + (labels[obj])* torch::pow(torch::clamp(1.0 - output[obj], 0.0), 2);
			//	loss = torch::cat({ loss, tensor_buf }, 0);
			//}

			//loss = torch::mean(loss);


			auto loss = (1 - labels) * torch::pow(output, 2) + (labels)* torch::pow(torch::clamp(1.0 - output, 0.0), 2);
			loss = torch::mean(loss);

			loss.backward();
			optimizer.step();

			mse += loss.template item<float>();

			optimizer.zero_grad();

			count++;
		}
		std::cout << "\r";


		mse /= (float)count;

		std::cout << "Train Epoch: " << epoch << "/" << epochs <<
		" Mean squared error: " << mse << " Validation data ";

		model->eval();
		siam_test(data_val, model);
		model->train();

		auto end = std::chrono::steady_clock::now();
		auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
		std::cout << "Time for epoch: " << elapsed_ms.count() << " ms\n";

		if (mse < best_mse)
		{
			model->to(torch::kCPU);
			model->eval();
			torch::save(model, path_save_NN);
			best_mse = mse;
			std::cout << "model save" << std::endl;
			if (epoch != epochs) {
				model->to(device);
				model->train();
			}
		}
	}
}

void siam_classification(std::string path_img_1, std::string path_img_2, std::string path_NN) {
	auto img_1 = img_to_tensor(path_img_1);
	auto img_2 = img_to_tensor(path_img_2);

	ConvNet model(3, 64, 64);
	torch::load(model, path_NN);

	model->eval();

	torch::Tensor result = model->forward(img_1, img_2);

	if(result.template item<float>() < 0.5)
		std::cout << "identical: " << 1.0 - result.template item<float>() << std::endl;
	else
		std::cout << "no identical: " << result.template item<float>()  << std::endl;
}

void siam_test(Siam_data_set data_test, ConvNet model){
	int error = 0;

	for (int i = 0; i < data_test.size(); i++) {
		auto data = data_test.get(i);
		auto out_model = model->forward(data.img_1, data.img_2);

		if ((out_model.template item<float>() < 0.5 && data.label.template item<int>() != 0)||
			(out_model.template item<float>() >= 0.5 && data.label.template item<int>() != 1)) {
			error++;
		}
	}

	std::cout << "error: " << (float)error/data_test.size() <<std::endl;
}



