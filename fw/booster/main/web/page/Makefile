all:
	@npm run build
	@sed -e "s/<script src=\"index.js\"><\/script>//g" -i dist/index.html
	@rm dist/*.js
	@du -h dist/index.html

modules_install:
	@npm i --save-dev html-webpack-plugin
	@npm i html-inline-css-webpack-plugin -D