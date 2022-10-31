const minify = require('html-minifier').minify;
const fs = require('fs');

try {

  let target = fs.readFileSync('./src/mode/ap/index.html');
  target = target.toString('utf-8');
  
  const result = minify(target, {
    minifyCSS: true,
    minifyJS: true,
    collapseWhitespace: true,
    removeComments: true,
  });
  
  fs.writeFile('./data/index.minify.html', result, err => {
    if (err) {
      console.error(err);
    }
    // file written successfully
    console.error('File has been saved!');
  });
  
} catch (error) {
  console.log('[Detach Error] When compress file');
}